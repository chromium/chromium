// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.util.ArrayMap;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Manages the UI for tab sharing toolbars for each window. Observes the global {@link
 * TabSharingUiManager} for active tab sharing sessions to add or remove its corresponding toolbar
 * by the {@link TabSharingToolbarCoordinator}.
 */
@NullMarked
public class TabSharingToolbarUiCoordinator implements TabSharingUiManager.Observer {
    private final Context mContext;
    private final ViewGroup mParentView;
    private final TabSharingToolbarContainer mContainer;
    private final TopControlsStacker mTopControlsStacker;
    private final ActivityTabProvider mTabProvider;

    // One child coordinator (and toolbar view) per active sharing session in this window. A window
    // can host multiple concurrent sessions, whose toolbars are stacked in mContainer; keyed by the
    // session's bridge. (The OS currently allows only one active session; see crbug.com/487666920.)
    private final Map<TabSharingUiBridge, TabSharingToolbarCoordinator> mCoordinators =
            new ArrayMap<>();

    // Safety timeout after which a toolbar retained for an in-progress source switch is torn down
    // if the replacement session never arrives (e.g. the switch was aborted).
    private static final long SOURCE_SWITCH_TIMEOUT_MS = 1500;

    // Toolbars kept alive across a source switch, keyed by capturer, awaiting the new session's
    // toolbar so they can be swapped in place instead of removed and re-added (which would bounce
    // the top controls height).
    private final Map<WebContents, PendingSwap> mPendingSwaps = new ArrayMap<>();

    private static class PendingSwap {
        public final TabSharingToolbarCoordinator coordinator;
        public final Runnable timeout;

        PendingSwap(TabSharingToolbarCoordinator coordinator, Runnable timeout) {
            this.coordinator = coordinator;
            this.timeout = timeout;
        }
    }

    /**
     * Initializes the UI coordinator, which attaches the toolbar container to the provided {@code
     * parentView} and registers as an observer to manage sharing sessions for this window.
     *
     * @param context The Android context.
     * @param parentView The control container parent view to add the tab sharing toolbar view to.
     * @param stacker The stacker that manages top controls.
     * @param tabProvider The provider of the current tab.
     */
    public TabSharingToolbarUiCoordinator(
            Context context,
            ViewGroup parentView,
            TopControlsStacker stacker,
            ActivityTabProvider tabProvider) {
        mContext = context;
        mParentView = parentView;
        mTopControlsStacker = stacker;
        mTabProvider = tabProvider;

        mContainer = new TabSharingToolbarContainer(context, stacker);
        mTopControlsStacker.addControl(mContainer);

        LayoutParams lp =
                new LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setLayoutParams(lp);
        parentView.addView(mContainer);

        TabSharingUiManager.getInstance().addObserver(this);
    }

    /**
     * Cleans up the coordinator by unregistering from global session updates and destroying any
     * active toolbars within this window.
     */
    public void destroy() {
        TabSharingUiManager.getInstance().removeObserver(this);

        for (TabSharingToolbarCoordinator coordinator : mCoordinators.values()) {
            coordinator.destroy();
        }
        mCoordinators.clear();

        // Tear down any toolbars retained for in-progress source switches.
        for (PendingSwap pending : mPendingSwaps.values()) {
            ThreadUtils.getUiThreadHandler().removeCallbacks(pending.timeout);
            pending.coordinator.destroy();
        }
        mPendingSwaps.clear();

        mTopControlsStacker.removeControl(mContainer);
        mContainer.destroy();
        mParentView.removeView(mContainer);
    }

    @Override
    public void onSharingSessionStarted(TabSharingUiBridge bridge) {
        // Only show a session's toolbar in windows whose profile matches the session's incognito
        // state. This prevents an incognito session's info (e.g. the shared URL) from leaking into
        // normal windows, and vice versa.
        Tab currentTab = mTabProvider.get();
        Profile windowProfile = currentTab != null ? currentTab.getProfile() : null;
        Profile sessionProfile = Profile.fromWebContents(bridge.getCapturer());
        if (windowProfile != null
                && sessionProfile != null
                && windowProfile.isOffTheRecord() != sessionProfile.isOffTheRecord()) {
            return;
        }

        TabSharingToolbarCoordinator coordinator =
                new TabSharingToolbarCoordinator(mContext, bridge, mTabProvider);
        mCoordinators.put(bridge, coordinator);

        // If this session is the continuation of a source switch, swap the retained old toolbar for
        // the new one in place so the reported height is unchanged (no web-contents jump).
        PendingSwap pending = mPendingSwaps.remove(bridge.getCapturer());
        if (pending != null) {
            ThreadUtils.getUiThreadHandler().removeCallbacks(pending.timeout);
            mContainer.swapToolbar(pending.coordinator.getView(), coordinator.getView());
            pending.coordinator.destroy();
        } else {
            mContainer.addToolbar(coordinator.getView());
        }
    }

    @Override
    public void onSharingSessionStopped(TabSharingUiBridge bridge) {
        TabSharingToolbarCoordinator coordinator = mCoordinators.remove(bridge);
        if (coordinator == null) return;

        // During a source switch the old session is immediately followed by a new one for the same
        // capturer. Retain the toolbar and swap it when the new session starts (see
        // onSharingSessionStarted) so the container's height never collapses to zero and the web
        // contents don't jump. A safety timeout releases it if the switch is aborted and no new
        // session arrives.
        WebContents capturer = bridge.getCapturer();
        if (MediaCaptureDevicesDispatcherAndroid.isSourceSwitchingInProgress(capturer)) {
            // Defensive: drop any stale pending swap for this capturer before retaining a new one.
            releasePendingSwap(capturer);
            Runnable timeout =
                    () -> {
                        releasePendingSwap(capturer);
                        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(
                                capturer, false);
                    };
            mPendingSwaps.put(capturer, new PendingSwap(coordinator, timeout));
            ThreadUtils.getUiThreadHandler().postDelayed(timeout, SOURCE_SWITCH_TIMEOUT_MS);
            return;
        }

        mContainer.removeToolbar(coordinator.getView());
        coordinator.destroy();
    }

    /**
     * Tears down a toolbar retained for an in-progress source switch that never completed (e.g. the
     * switch was aborted), collapsing the reserved height.
     */
    private void releasePendingSwap(WebContents capturer) {
        PendingSwap pending = mPendingSwaps.remove(capturer);
        if (pending == null) return;
        ThreadUtils.getUiThreadHandler().removeCallbacks(pending.timeout);
        mContainer.removeToolbar(pending.coordinator.getView());
        pending.coordinator.destroy();
    }
}
