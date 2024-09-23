// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.feature_engagement.Tracker;

/** Root coordinator of the Hub. */
public class HubCoordinator implements PaneHubController, BackPressHandler {
    private final @NonNull FrameLayout mContainerView;
    private final @NonNull View mMainHubParent;
    private final @NonNull PaneManager mPaneManager;
    private final @NonNull HubToolbarCoordinator mHubToolbarCoordinator;
    private final @NonNull HubPaneHostCoordinator mHubPaneHostCoordinator;
    private final @NonNull HubLayoutController mHubLayoutController;
    private final @NonNull ObservableSupplierImpl<Boolean> mHandleBackPressSupplier;

    /**
     * Generic callback that invokes {@link #updateHandleBackPressSupplier()}. This can be cast to
     * an arbitrary {@link Callback} and the provided value is discarded.
     */
    private final @NonNull Callback<Object> mBackPressStateChangeCallback;

    /**
     * Warning: {@link #getFocusedPane()} may return null if no pane is focused or {@link
     * Pane#getHandleBackPressChangedSupplier()} contains null.
     */
    private final @NonNull TransitiveObservableSupplier<Pane, Boolean>
            mFocusedPaneHandleBackPressSupplier;

    private final @NonNull PaneBackStackHandler mPaneBackStackHandler;
    private final @NonNull ObservableSupplier<Tab> mCurrentTabSupplier;

    /**
     * Creates the {@link HubCoordinator}.
     *
     * @param activity The Android activity context.
     * @param profileProviderSupplier Used to fetch dependencies.
     * @param containerView The view to attach the Hub to.
     * @param paneManager The {@link PaneManager} for Hub.
     * @param hubLayoutController The controller of the {@link HubLayout}.
     * @param currentTabSupplier The supplier of the current {@link Tab}.
     * @param menuButtonCoordinator Root component for the app menu.
     * @param edgeToEdgeSupplier The supplier of {@link EdgeToEdgeController}.
     * @param searchActivityClient A client for the search activity, used to launch search.
     */
    public HubCoordinator(
            @NonNull Activity activity,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull FrameLayout containerView,
            @NonNull PaneManager paneManager,
            @NonNull HubLayoutController hubLayoutController,
            @NonNull ObservableSupplier<Tab> currentTabSupplier,
            @NonNull MenuButtonCoordinator menuButtonCoordinator,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull SearchActivityClient searchActivityClient) {
        Context context = containerView.getContext();
        mBackPressStateChangeCallback = (ignored) -> updateHandleBackPressSupplier();
        mPaneManager = paneManager;
        mFocusedPaneHandleBackPressSupplier =
                new TransitiveObservableSupplier<>(
                        paneManager.getFocusedPaneSupplier(),
                        p -> p.getHandleBackPressChangedSupplier());
        mFocusedPaneHandleBackPressSupplier.addObserver(
                castCallback(mBackPressStateChangeCallback));

        mContainerView = containerView;
        mMainHubParent = LayoutInflater.from(context).inflate(R.layout.hub_layout, null);
        mContainerView.addView(mMainHubParent);

        ProfileProvider profileProvider = profileProviderSupplier.get();
        assert profileProvider != null;
        Tracker tracker = TrackerFactory.getTrackerForProfile(profileProvider.getOriginalProfile());
        HubToolbarView hubToolbarView = mContainerView.findViewById(R.id.hub_toolbar);
        mHubToolbarCoordinator =
                new HubToolbarCoordinator(
                        activity,
                        hubToolbarView,
                        paneManager,
                        menuButtonCoordinator,
                        tracker,
                        searchActivityClient);

        HubPaneHostView hubPaneHostView = mContainerView.findViewById(R.id.hub_pane_host);
        mHubPaneHostCoordinator =
                new HubPaneHostCoordinator(
                        hubPaneHostView, paneManager.getFocusedPaneSupplier(), edgeToEdgeSupplier);

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
                && getFocusedPane().handleBackPress() == BackPressResult.SUCCESS) {
            return BackPressResult.SUCCESS;
        }

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

    @Override
    public void selectTabAndHideHub(int tabId) {
        mHubLayoutController.selectTabAndHideHubLayout(tabId);
    }

    @Override
    public void focusPane(@PaneId int paneId) {
        mPaneManager.focusPane(paneId);
    }

    @Nullable
    @Override
    public View getPaneButton(@PaneId int paneId) {
        return mHubToolbarCoordinator.getPaneButton(paneId);
    }

    @Nullable
    @Override
    public View getFloatingActionButton() {
        return mHubPaneHostCoordinator.getFloatingActionButton();
    }

    /** Returns the view group to contain the snackbar. */
    public ViewGroup getSnackbarContainer() {
        return mHubPaneHostCoordinator.getSnackbarContainer();
    }

    private @Nullable Pane getFocusedPane() {
        return mPaneManager.getFocusedPaneSupplier().get();
    }

    private void updateHandleBackPressSupplier() {
        boolean shouldHandleBackPress =
                Boolean.TRUE.equals(mFocusedPaneHandleBackPressSupplier.get())
                        || mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                        || (mCurrentTabSupplier.get() != null);
        mHandleBackPressSupplier.set(shouldHandleBackPress);
    }

    private <T> Callback<T> castCallback(Callback callback) {
        return (Callback<T>) callback;
    }
}
