// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.util.ArrayMap;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Map;

/**
 * Manages the UI for tab sharing toolbars for each window. Observes the global {@link
 * TabSharingUIManager} for active tab sharing sessions to add or remove its corresponding toolbar
 * by the {@link TabSharingToolbarCoordinator}.
 */
// TODO(crbug.com/546411766): Rename TabSharingUIManager and TabSharingUIBridge to
// TabSharingUiManager/TabSharingUiBridge to follow Java acronym-casing conventions.
@NullMarked
public class TabSharingToolbarUiCoordinator implements TabSharingUIManager.Observer {
    private final Context mContext;
    private final ViewGroup mParentView;
    private final TabSharingToolbarContainer mContainer;
    private final TopControlsStacker mTopControlsStacker;
    private final ActivityTabProvider mTabProvider;

    // One child coordinator (and toolbar view) per active sharing session in this window. A window
    // can host multiple concurrent sessions, whose toolbars are stacked in mContainer; keyed by the
    // session's bridge. (The OS currently allows only one active session; see crbug.com/487666920.)
    private final Map<TabSharingUIBridge, TabSharingToolbarCoordinator> mCoordinators =
            new ArrayMap<>();

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

        TabSharingUIManager.getInstance().addObserver(this);
    }

    /**
     * Cleans up the coordinator by unregistering from global session updates and destroying any
     * active toolbars within this window.
     */
    public void destroy() {
        TabSharingUIManager.getInstance().removeObserver(this);

        for (TabSharingToolbarCoordinator coordinator : mCoordinators.values()) {
            coordinator.destroy();
        }
        mCoordinators.clear();

        mTopControlsStacker.removeControl(mContainer);
        mContainer.destroy();
        mParentView.removeView(mContainer);
    }

    @Override
    public void onSharingSessionStarted(TabSharingUIBridge bridge) {
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
        mContainer.addToolbar(coordinator.getView());
    }

    @Override
    public void onSharingSessionStopped(TabSharingUIBridge bridge) {
        TabSharingToolbarCoordinator coordinator = mCoordinators.remove(bridge);
        if (coordinator != null) {
            mContainer.removeToolbar(coordinator.getView());
            coordinator.destroy();
        }
    }
}
