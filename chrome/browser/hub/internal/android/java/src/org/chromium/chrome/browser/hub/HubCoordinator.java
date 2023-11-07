// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/** Root coordinator of the Hub. */
public class HubCoordinator implements BackPressHandler {
    private final @NonNull FrameLayout mContainerView;
    private final @NonNull View mMainHubParent;
    private final @NonNull HubToolbarCoordinator mHubToolbarCoordinator;
    private final @NonNull HubPaneHostCoordinator mHubPaneHostCoordinator;
    private final @NonNull HubLayoutController mHubLayoutController;
    private final @NonNull ObservableSupplierImpl<Boolean> mHandleBackPressSupplier;
    private final @NonNull PaneBackStackHandler mPaneBackStackHandler;
    private final @NonNull ObservableSupplier<Tab> mCurrentTabSupplier;

    /**
     * Creates the {@link HubCoordinator}.
     *
     * @param containerView The view to attach the Hub to.
     * @param paneSupplier Used to observe the current pane.
     * @param hubLayoutController The controller of the {@link HubLayout}.
     * @param currentTabSupplier The supplier of the current {@link Tab}.
     */
    public HubCoordinator(
            @NonNull FrameLayout containerView,
            @NonNull PaneManager paneManager,
            @NonNull HubLayoutController hubLayoutController,
            @NonNull ObservableSupplier<Tab> currentTabSupplier) {
        Context context = containerView.getContext();
        mContainerView = containerView;
        mMainHubParent = LayoutInflater.from(context).inflate(R.layout.hub_layout, null);
        mContainerView.addView(mMainHubParent);

        HubToolbarView hubToolbarView = mContainerView.findViewById(R.id.hub_toolbar);
        mHubToolbarCoordinator = new HubToolbarCoordinator(hubToolbarView);

        HubPaneHostView hubPaneHostView = mContainerView.findViewById(R.id.hub_pane_host);
        mHubPaneHostCoordinator =
                new HubPaneHostCoordinator(hubPaneHostView, paneManager.getFocusedPaneSupplier());

        mHubLayoutController = hubLayoutController;
        mHandleBackPressSupplier = new ObservableSupplierImpl<>();

        mPaneBackStackHandler = new PaneBackStackHandler(paneManager);
        mPaneBackStackHandler
                .getHandleBackPressChangedSupplier()
                .addObserver((handlesBackPress) -> updateHandleBackPressSupplier());

        mCurrentTabSupplier = currentTabSupplier;
        mCurrentTabSupplier.addObserver((tab) -> updateHandleBackPressSupplier());

        updateHandleBackPressSupplier();
    }

    /** Removes the hub from the layout tree and cleans up resources. */
    public void destroy() {
        mContainerView.removeView(mMainHubParent);
        mHubToolbarCoordinator.destroy();
        mHubPaneHostCoordinator.destroy();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        // TODO(crbug/1498614): Add support here for in order of priority.
        // 1) Delegate to the current Pane.
        // 2) Delegate to PaneBackStackHandler. - DONE
        // 3) No-op if Start Surface was the previous layout. It should be higher priority and
        //    already handle it, but add verification.
        // 4) Hide the Hub to the most recent tab in the current TabModel. - DONE.
        if (mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                && mPaneBackStackHandler.handleBackPress() == BackPressResult.SUCCESS) {
            return BackPressResult.SUCCESS;
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

    private void updateHandleBackPressSupplier() {
        // TODO(crbug/1498614): See comment in handleBackPress. The conditions to check for each
        // case are:
        // 1) Whether the Pane's getHandleBackPressChangedSupplier is set.
        // 2) Whether the PaneBackStackHandler getHandleBackPressChangeSupplier is set. - DONE
        // 3) Whether Start Surface was the previous layout and we are not in incognito mode.
        // 4) Whether the current TabModel has a selected tab. - DONE
        boolean shouldHandleBackPress =
                mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                        || mCurrentTabSupplier.get() != null;
        mHandleBackPressSupplier.set(shouldHandleBackPress);
    }
}
