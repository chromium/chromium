// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/** Root coordinator of the Hub. */
public class HubCoordinator implements BackPressHandler {
    private static final Integer START_SURFACE_LAYOUT_TYPE =
            Integer.valueOf(LayoutType.START_SURFACE);

    private final @NonNull FrameLayout mContainerView;
    private final @NonNull View mMainHubParent;
    private final @NonNull HubToolbarCoordinator mHubToolbarCoordinator;
    private final @NonNull HubPaneHostCoordinator mHubPaneHostCoordinator;
    private final @NonNull HubLayoutController mHubLayoutController;
    private final @NonNull ObservableSupplierImpl<Boolean> mHandleBackPressSupplier;
    private final @NonNull ObservableSupplier<Pane> mFocusedPaneSupplier;

    /**
     * Generic callback that invokes {@link #updateHandleBackPressSupplier()}. This can be cast to
     * an arbitrary {@link Callback} and the provided value is discarded.
     */
    private final @NonNull Callback<Object> mBackPressStateChangeCallback;

    /**
     * Warning: {@link mFocusedPaneSupplier#get()} may return null if no pane is focused or {@link
     * Pane#getHandleBackPressChangedSupplier()} contains null.
     */
    private final @NonNull TransitiveObservableSupplier<Pane, Boolean>
            mFocusedPaneHandleBackPressSupplier;

    private final @NonNull PaneBackStackHandler mPaneBackStackHandler;
    private final @NonNull ObservableSupplier<Tab> mCurrentTabSupplier;

    /**
     * Creates the {@link HubCoordinator}.
     *
     * @param containerView The view to attach the Hub to.
     * @param paneManager The {@link PaneManager} for Hub.
     * @param hubLayoutController The controller of the {@link HubLayout}.
     * @param currentTabSupplier The supplier of the current {@link Tab}.
     */
    public HubCoordinator(
            @NonNull FrameLayout containerView,
            @NonNull PaneManager paneManager,
            @NonNull HubLayoutController hubLayoutController,
            @NonNull ObservableSupplier<Tab> currentTabSupplier) {
        Context context = containerView.getContext();
        mBackPressStateChangeCallback = (ignored) -> updateHandleBackPressSupplier();
        mFocusedPaneSupplier = paneManager.getFocusedPaneSupplier();
        mFocusedPaneHandleBackPressSupplier =
                new TransitiveObservableSupplier<>(
                        mFocusedPaneSupplier, p -> p.getHandleBackPressChangedSupplier());
        mFocusedPaneHandleBackPressSupplier.addObserver(
                castCallback(mBackPressStateChangeCallback));

        mContainerView = containerView;
        mMainHubParent = LayoutInflater.from(context).inflate(R.layout.hub_layout, null);
        mContainerView.addView(mMainHubParent);

        HubToolbarView hubToolbarView = mContainerView.findViewById(R.id.hub_toolbar);
        mHubToolbarCoordinator = new HubToolbarCoordinator(hubToolbarView, paneManager);

        HubPaneHostView hubPaneHostView = mContainerView.findViewById(R.id.hub_pane_host);
        mHubPaneHostCoordinator =
                new HubPaneHostCoordinator(hubPaneHostView, paneManager.getFocusedPaneSupplier());

        mHubLayoutController = hubLayoutController;
        mHandleBackPressSupplier = new ObservableSupplierImpl<>();

        mPaneBackStackHandler = new PaneBackStackHandler(paneManager);
        mPaneBackStackHandler
                .getHandleBackPressChangedSupplier()
                .addObserver(castCallback(mBackPressStateChangeCallback));

        mCurrentTabSupplier = currentTabSupplier;
        mCurrentTabSupplier.addObserver(castCallback(mBackPressStateChangeCallback));

        mHubLayoutController
                .getPreviousLayoutTypeSupplier()
                .addObserver(castCallback(mBackPressStateChangeCallback));

        updateHandleBackPressSupplier();
    }

    /** Removes the hub from the layout tree and cleans up resources. */
    public void destroy() {
        mContainerView.removeView(mMainHubParent);

        mFocusedPaneHandleBackPressSupplier.removeObserver(
                castCallback(mBackPressStateChangeCallback));
        mCurrentTabSupplier.removeObserver(castCallback(mBackPressStateChangeCallback));
        mHubLayoutController
                .getPreviousLayoutTypeSupplier()
                .removeObserver(castCallback(mBackPressStateChangeCallback));
        mPaneBackStackHandler
                .getHandleBackPressChangedSupplier()
                .removeObserver(castCallback(mBackPressStateChangeCallback));
        mPaneBackStackHandler.destroy();

        mHubToolbarCoordinator.destroy();
        mHubPaneHostCoordinator.destroy();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        if (Boolean.TRUE.equals(mFocusedPaneHandleBackPressSupplier.get())
                && mFocusedPaneSupplier.get().handleBackPress() == BackPressResult.SUCCESS) {
            return BackPressResult.SUCCESS;
        }

        if (mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                && mPaneBackStackHandler.handleBackPress() == BackPressResult.SUCCESS) {
            return BackPressResult.SUCCESS;
        }

        // TODO(crbug/1498614): Discuss with Start Surface owners and investigate removing.
        if (startSurfaceHandlesBackpress()) {
            // This is based on the logic in TabSwitcherMediator where the logic is delegated to
            // ReturnToChromeBackPressHandler.
            return BackPressResult.FAILURE;
        }

        Tab tab = mCurrentTabSupplier.get();
        if (tab != null) {
            mHubLayoutController.selectTabAndHideHubLayout(tab.getId());
            return BackPressResult.SUCCESS;
        }
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressSupplier;
    }

    private boolean startSurfaceHandlesBackpress() {
        Tab currentTab = mCurrentTabSupplier.get();
        boolean isIncognito = currentTab != null ? currentTab.isIncognito() : false;
        return !isIncognito
                && START_SURFACE_LAYOUT_TYPE.equals(
                        mHubLayoutController.getPreviousLayoutTypeSupplier().get());
    }

    private void updateHandleBackPressSupplier() {
        boolean shouldHandleBackPress =
                Boolean.TRUE.equals(mFocusedPaneHandleBackPressSupplier.get())
                        || mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                        || (!startSurfaceHandlesBackpress() && mCurrentTabSupplier.get() != null);
        mHandleBackPressSupplier.set(shouldHandleBackPress);
    }

    private <T> Callback<T> castCallback(Callback callback) {
        return (Callback<T>) callback;
    }
}
