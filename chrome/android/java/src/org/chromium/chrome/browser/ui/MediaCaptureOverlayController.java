// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsOffsetHelper;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the visibility of an overlay border when tab capture is ongoing.
 * The border will be visible if any of the captured/tracked tabs are user interactible
 * (e.g. visible with no overlays hiding the content), and hidden otherwise. It attempts
 * to respond to control state events to resize the UI as the size of the currently
 * visible captured tab is changed.
 */
public class MediaCaptureOverlayController implements UnownedUserData {
    private static final UnownedUserDataKey<MediaCaptureOverlayController> KEY =
            new UnownedUserDataKey<MediaCaptureOverlayController>(
                    MediaCaptureOverlayController.class);

    private final CaptureOverlayTabObserver mTabObserver = new CaptureOverlayTabObserver();

    private View mOverlayView;
    private SparseArray<Tab> mCapturedTabs = new SparseArray<Tab>();
    private Tab mVisibleTab;

    private class CaptureOverlayTabObserver extends EmptyTabObserver {
        /**
         * When the Tab Switcher UI is summoned this is fired; though this does
         * not get fired when the omnibox appears (that is currently handled due
         * to the positioning of the element in the view hierarchy). The tab
         * remains not interactible until it is switched back to.
         */
        @Override
        public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
            // There are two cases that we need to handle:
            // 1) The currently visible tab is becoming invisible
            // 2) A new tab is becoming visible
            if (isInteractable && tab != mVisibleTab) {
                setVisibleTab(tab);
            } else if (!isInteractable && tab == mVisibleTab) {
                clearVisibleTab();
            }
        }

        @Override
        public void onBrowserControlsOffsetChanged(
                Tab tab,
                int topControlsOffsetY,
                int bottomControlsOffsetY,
                int contentOffsetY,
                int topControlsMinHeightOffsetY,
                int bottomControlsMinHeightOffsetY) {
            if (tab == mVisibleTab) updateMargins();
        }

        @Override
        public void onDestroyed(Tab tab) {
            stopCapture(tab);
        }
    }

    /**
     * Get the Activity's {@link MediaCaptureOverlayController} from the provided {@link
     * WindowAndroid}.
     * @param window The window to get the manager from.
     * @return The Activity's {@link MediaCaptureOverlayController}.
     */
    public static @Nullable MediaCaptureOverlayController from(WindowAndroid window) {
        if (window == null) return null;
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * Make this instance of MediaCaptureOverlayController available through the activity's window.
     *
     * @param window A {@link WindowAndroid} to attach to.
     */
    private static void attach(WindowAndroid window, MediaCaptureOverlayController overlay) {
        KEY.attachToHost(window.getUnownedUserDataHost(), overlay);
    }

    /** Detach the provided MediaCaptureOverlayController from any host it is associated with. */
    private static void detach(MediaCaptureOverlayController overlay) {
        KEY.detachFromAllHosts(overlay);
    }

    public MediaCaptureOverlayController(WindowAndroid window, View overlayView) {
        mOverlayView = overlayView;
        attach(window, this);
    }

    /**
     * Mark that the provided {@link Tab} is being captured and begin tracking it's state
     * to determine whether and at what size the CaptureOverlay needs to be shown.
     * @param capturedTab A {@link Tab} which is being captured and should have the overlay shown
     *         while it is active.
     */
    public void startCapture(Tab capturedTab) {
        int tabId = capturedTab.getId();
        if (mCapturedTabs.indexOfKey(tabId) >= 0) return;

        capturedTab.addObserver(mTabObserver);
        mCapturedTabs.put(tabId, capturedTab);

        if (capturedTab.isUserInteractable()) setVisibleTab(capturedTab);
    }

    /**
     * Mark that the provided {@link Tab} is no longer being captured, and that the capture
     * overlay should no longer appear when it is active.
     * @param capturedTab A {@link Tab} which is no longer being captured and should have the
     *         overlay hidden while it is active.
     */
    public void stopCapture(Tab capturedTab) {
        int tabId = capturedTab.getId();
        if (mCapturedTabs.indexOfKey(tabId) < 0) return;

        capturedTab.removeObserver(mTabObserver);
        mCapturedTabs.remove(tabId);

        if (capturedTab == mVisibleTab) clearVisibleTab();
    }

    /**
     * Mark that the provided tab is the visible tab. This is the only tab that will be referenced
     * when determining the size of the overlay to show. This also forces the overlay visible if it
     * is not.
     */
    private void setVisibleTab(@NonNull Tab tab) {
        mVisibleTab = tab;
        updateMargins();
        mOverlayView.setVisibility(View.VISIBLE);
    }

    /** Mark that the current visible tab is no longer visible and immediately hide the overlay. */
    private void clearVisibleTab() {
        mOverlayView.setVisibility(View.GONE);
        mVisibleTab = null;
    }

    /**
     * If we have an overlay, update the margins/size of the overlay border to ensure that they
     * surround the tab content.
     */
    private void updateMargins() {
        if (mVisibleTab == null) return;

        TabBrowserControlsOffsetHelper offsetHelper =
                TabBrowserControlsOffsetHelper.get(mVisibleTab);
        if (!offsetHelper.offsetInitialized()) return;

        MarginLayoutParams params = (MarginLayoutParams) mOverlayView.getLayoutParams();
        params.topMargin = offsetHelper.contentOffset();
        params.bottomMargin = offsetHelper.bottomControlsOffset();
        mOverlayView.setLayoutParams(params);
    }

    /**
     * Called by the {@link RootUiCoordinator}/owner of this object, when it should no longer be
     * used or queryable. Any overlays will be immediately hidden, and all tracked tabs will be
     * unsubscribed from.
     */
    public void destroy() {
        for (int i = 0; i < mCapturedTabs.size(); i++) {
            mCapturedTabs.valueAt(i).removeObserver(mTabObserver);
        }
        mCapturedTabs.clear();
        clearVisibleTab();
        mOverlayView = null;
        detach(this);
    }
}
