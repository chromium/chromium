// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Actor Overlay. */
@NullMarked
class ActorOverlayMediator
        implements ActorUiTabController.Observer,
                LayoutStateProvider.LayoutStateObserver,
                BackPressHandler {
    private final PropertyModel mModel;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final TabObserver mTabObserver;
    private final Callback<@Nullable Tab> mCurrentTabObserver;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private final TabObscuringHandler mTabObscuringHandler;
    private final MonotonicObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private final Callback<LayoutManager> mLayoutManagerAvailableCallback;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressChangedSupplier =
            ObservableSuppliers.createNonNull(false);
    private final Runnable mBackPressCallback;

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
     * @param backPressCallback The callback to show the snackbar.
     */
    public ActorOverlayMediator(
            PropertyModel model,
            TabModelSelector tabModelSelector,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier,
            Runnable backPressCallback) {
        mModel = model;
        mCurrentTabSupplier = tabModelSelector.getCurrentTabSupplier();
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mTabObscuringHandler = tabObscuringHandler;
        mLayoutManagerSupplier = layoutManagerSupplier;
        mBackPressCallback = backPressCallback;

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onShown(Tab tab, int type) {
                        updateCanShowOverlay(tab);
                    }

                    @Override
                    public void onHidden(Tab tab, int reason) {
                        setCanShow(false);
                    }
                };

        mCurrentTabObserver = this::onCurrentTabChanged;
        mCurrentTabSupplier.addSyncObserverAndCallIfNonNull(mCurrentTabObserver);

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
        updateCanShowOverlay(mCurrentTabSupplier.get());
    }

    @Override
    public void onStartedShowing(int layoutType) {
        updateCanShowOverlay(mCurrentTabSupplier.get());
    }

    @Override
    public void onUiTabStateChanged(ActorUiTabController.UiTabState state) {
        setOverlayVisible(state.actorOverlay.isActive);
    }

    private void onCurrentTabChanged(@Nullable Tab tab) {
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
            if (mTabController != null) {
                mTabController.removeObserver(this);
            }
        }

        mCurrentTab = tab;
        mTabController = null;

        if (mCurrentTab != null) {
            mCurrentTab.addObserver(mTabObserver);
        }

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
                        && !tab.isNativePage()
                        && !tab.isHidden();
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

        mBackPressChangedSupplier.set(isVisible);

        if (isVisible && mTabObscuringToken == null) {
            mTabObscuringToken = mTabObscuringHandler.obscure(TabObscuringHandler.Target.TAB_CONTENT);
        } else if (mTabObscuringToken != null) {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
    }

    @Override
    public int handleBackPress() {
        mBackPressCallback.run();
        return BackPressResult.SUCCESS;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    /** Cleans up the mediator. */
    public void destroy() {
        if (mTabController != null) {
            mTabController.removeObserver(this);
            mTabController = null;
        }
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
            mCurrentTab = null;
        }
        if (mTabObscuringToken != null) {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
        mCurrentTabSupplier.removeObserver(mCurrentTabObserver);
        mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
        mLayoutManagerSupplier.removeObserver(mLayoutManagerAvailableCallback);
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(this);
        }
    }
}
