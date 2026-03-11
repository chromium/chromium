// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Actor Overlay. */
@NullMarked
class ActorOverlayMediator
        implements ActorUiTabController.Observer, LayoutStateProvider.LayoutStateObserver {
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final Callback<@Nullable Tab> mCurrentTabObserver;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private final TabObscuringHandler mTabObscuringHandler;
    private final MonotonicObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private final Callback<LayoutManager> mLayoutManagerAvailableCallback;

    private @Nullable Tab mCurrentTab;
    private @Nullable ActorUiTabController mTabController;
    private @Nullable LayoutManager mLayoutManager;
    private TabObscuringHandler.@Nullable Token mTabObscuringToken;

    /**
     * @param model The PropertyModel to modify.
     * @param tabModelSelector The TabModelSelector to observe.
     * @param browserControlsVisibilityManager The BrowserControlsVisibilityManager to observe.
     * @param tabObscuringHandler The TabObscuringHandler to obscure the web content.
     * @param layoutManagerSupplier The LayoutManager supplier to observe layout changes.
     */
    public ActorOverlayMediator(
            PropertyModel model,
            TabModelSelector tabModelSelector,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier) {
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mTabObscuringHandler = tabObscuringHandler;
        mLayoutManagerSupplier = layoutManagerSupplier;

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabHidden(Tab tab) {
                        setCanShow(false);
                    }
                };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mCurrentTabObserver = this::onCurrentTabChanged;
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

        mLayoutManagerAvailableCallback = this::onLayoutManagerAvailable;
        mLayoutManagerSupplier.addSyncObserverAndCallIfNonNull(mLayoutManagerAvailableCallback);
    }

    private void onLayoutManagerAvailable(LayoutManager layoutManager) {
        mLayoutManager = layoutManager;
        mLayoutManager.addObserver(this);
        updateCanShowOverlay(mTabModelSelector.getCurrentTabSupplier().get());
    }

    @Override
    public void onStartedShowing(int layoutType) {
        updateCanShowOverlay(mTabModelSelector.getCurrentTabSupplier().get());
    }

    @Override
    public void onUiTabStateChanged(ActorUiTabController.UiTabState state) {
        setOverlayVisible(state.actorOverlay.isActive);
    }

    private void onCurrentTabChanged(@Nullable Tab tab) {
        if (mCurrentTab != null && mTabController != null) {
            mTabController.removeObserver(this);
        }

        mCurrentTab = tab;
        mTabController = null;

        if (mCurrentTab == null) {
            setCanShow(false);
            setOverlayVisible(false);
            return;
        }

        mTabController = ActorUiTabController.from(mCurrentTab);
        if (mTabController == null) {
            setCanShow(false);
            setOverlayVisible(false);
            return;
        }
        mTabController.addObserver(this);

        updateCanShowOverlay(mCurrentTab);
        ActorUiTabController.UiTabState state = mTabController.getUiTabState();
        if (state != null) {
            onUiTabStateChanged(state);
        } else {
            // Reset visibility if no state is available.
            setOverlayVisible(false);
        }
    }

    private void updateCanShowOverlay(@Nullable Tab tab) {
        // TODO(wenyufu): This is a placeholder. Update this based on whether the overlay can be
        // shown for the tab.
        boolean isBrowsing =
                mLayoutManager != null
                        && mLayoutManager.getActiveLayoutType() == LayoutType.BROWSING;
        boolean canShow =
                isBrowsing
                        && tab != null
                        && !tab.isDestroyed()
                        && !tab.isClosing()
                        && !tab.isNativePage();
        setCanShow(canShow);
    }

    private void setCanShow(boolean canShow) {
        mModel.set(ActorOverlayProperties.CAN_SHOW, canShow);
        updateObscuringState();
    }

    /**
     * Sets the visibility of the overlay.
     *
     * @param visible True to make the overlay visible, false to hide it.
     */
    public void setOverlayVisible(boolean visible) {
        mModel.set(ActorOverlayProperties.VISIBLE, visible);
        updateObscuringState();
    }

    private void updateObscuringState() {
        boolean isVisible =
                mModel.get(ActorOverlayProperties.VISIBLE)
                        && mModel.get(ActorOverlayProperties.CAN_SHOW);

        if (isVisible && mTabObscuringToken == null) {
            mTabObscuringToken = mTabObscuringHandler.obscure(TabObscuringHandler.Target.TAB_CONTENT);
        } else if (mTabObscuringToken != null) {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
    }

    /** Cleans up the mediator. */
    public void destroy() {
        if (mCurrentTab != null && mTabController != null) {
            mTabController.removeObserver(this);
            mTabController = null;
            mCurrentTab = null;
        }
        if (mTabObscuringToken != null) {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
        mTabModelSelector.getCurrentTabSupplier().removeObserver(mCurrentTabObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
        mLayoutManagerSupplier.removeObserver(mLayoutManagerAvailableCallback);
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(this);
        }
    }
}
