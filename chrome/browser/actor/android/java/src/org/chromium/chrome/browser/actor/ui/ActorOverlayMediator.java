// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Actor Overlay. */
@NullMarked
class ActorOverlayMediator {
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final Callback<@Nullable Tab> mCurrentTabObserver;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    /**
     * @param model The PropertyModel to modify.
     * @param tabModelSelector The TabModelSelector to observe.
     * @param browserControlsVisibilityManager The BrowserControlsVisibilityManager to observe.
     */
    public ActorOverlayMediator(
            PropertyModel model,
            TabModelSelector tabModelSelector,
            BrowserControlsVisibilityManager browserControlsVisibilityManager) {
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabHidden(Tab tab) {
                        mModel.set(ActorOverlayProperties.CAN_SHOW, false);
                    }
                };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mCurrentTabObserver =
                (tab) -> {
                    if (tab != null) {
                        updateCanShowOverlay(tab);
                    }
                };
        mTabModelSelector
                .getCurrentTabSupplier()
                .addSyncObserverAndCallIfNonNull(mCurrentTabObserver);

        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onTopControlsHeightChanged(
                            int topControlsHeight, int topControlsMinHeight) {
                        mModel.set(ActorOverlayProperties.TOP_MARGIN, topControlsHeight);
                    }

                    @Override
                    public void onBottomControlsHeightChanged(
                            int bottomControlsHeight, int bottomControlsMinHeight) {
                        mModel.set(ActorOverlayProperties.BOTTOM_MARGIN, bottomControlsHeight);
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mBrowserControlsObserver);
        mModel.set(
                ActorOverlayProperties.TOP_MARGIN,
                mBrowserControlsVisibilityManager.getTopControlsHeight());
        mModel.set(
                ActorOverlayProperties.BOTTOM_MARGIN,
                mBrowserControlsVisibilityManager.getBottomControlsHeight());
    }

    private void updateCanShowOverlay(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed() || tab.isClosing()) {
            mModel.set(ActorOverlayProperties.CAN_SHOW, false);
            return;
        }
        // TODO(wenyufu): This is a placeholder. Update this based on whether the overlay can be
        // shown for the tab.
        mModel.set(ActorOverlayProperties.CAN_SHOW, !tab.isNativePage());
    }

    /**
     * Sets the visibility of the overlay.
     *
     * @param visible True to make the overlay visible, false to hide it.
     */
    public void setOverlayVisible(boolean visible) {
        mModel.set(ActorOverlayProperties.VISIBLE, visible);
    }

    /** Cleans up the mediator. */
    public void destroy() {
        mTabModelSelector.getCurrentTabSupplier().removeObserver(mCurrentTabObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
    }
}
