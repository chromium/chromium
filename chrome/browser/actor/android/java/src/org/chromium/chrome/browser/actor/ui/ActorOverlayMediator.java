// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

    /**
     * @param model The PropertyModel to modify.
     * @param tabModelSelector The TabModelSelector to observe.
     */
    public ActorOverlayMediator(PropertyModel model, TabModelSelector tabModelSelector) {
        mModel = model;
        mTabModelSelector = tabModelSelector;

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
    }
}
