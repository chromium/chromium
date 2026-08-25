// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;

/** Helper for deferring navigation actions until after the Hub has finished hiding. */
@NullMarked
public class HubExitNavigationHelper implements LayoutStateObserver, Destroyable {
    private final LayoutStateProvider mLayoutStateProvider;
    private final HubManager mHubManager;
    private @Nullable Runnable mAction;
    private boolean mIsWaitingForHubExit;
    private boolean mIsDestroyed;

    /**
     * Creates a helper to coordinate navigation actions on Hub exit.
     *
     * @param layoutStateProvider Provider for checking and observing layout state transitions.
     * @param hubManager Manager for interacting with and controlling Hub state.
     */
    public HubExitNavigationHelper(LayoutStateProvider layoutStateProvider, HubManager hubManager) {
        mLayoutStateProvider = layoutStateProvider;
        mHubManager = hubManager;
    }

    /**
     * Executes the given action immediately if the Hub is not visible, or defers it until the Hub
     * has finished hiding after selecting the specified tab.
     *
     * <p>If an action is already deferred while waiting for the Hub to hide, subsequent calls
     * replace the deferred action with the new one (latest intent wins / single-slot coalescing),
     * without re-triggering the hide sequence.
     *
     * @param currentTab The currently active tab to select and return to.
     * @param action The navigation action to execute once the Hub is hidden (or immediately if not
     *     in Hub).
     */
    public void runOrDefer(Tab currentTab, Runnable action) {
        if (mIsDestroyed) return;

        if (mIsWaitingForHubExit) {
            // Fallback for when the Hub is dismissed externally without firing onFinishedHiding.
            if (!mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)) {
                cleanup();
                action.run();
                return;
            }
            mAction = action;
            return;
        }

        if (mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)) {
            mIsWaitingForHubExit = true;
            mAction = action;
            mLayoutStateProvider.addObserver(this);

            if (mLayoutStateProvider.isLayoutStartingToHide(LayoutType.HUB)) {
                return;
            }

            mHubManager.selectTabAndHideHub(currentTab.getId());
        } else {
            action.run();
        }
    }

    @Override
    public void onFinishedHiding(@LayoutType int layoutType) {
        if (layoutType == LayoutType.HUB) {
            Runnable action = mAction;
            mAction = null;
            cleanup();
            if (action != null) {
                action.run();
            }
        }
    }

    @Override
    public void destroy() {
        mIsDestroyed = true;
        mAction = null;
        cleanup();
    }

    private void cleanup() {
        mIsWaitingForHubExit = false;
        mLayoutStateProvider.removeObserver(this);
    }
}
