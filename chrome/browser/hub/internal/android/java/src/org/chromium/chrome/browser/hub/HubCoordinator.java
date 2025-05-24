// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.util.XrUtils;

/** Root coordinator of the Hub. */
@NullMarked
public class HubCoordinator implements PaneHubController, BackPressHandler {
    private final FrameLayout mContainerView;
    private final View mMainHubParent;
    private final PaneManager mPaneManager;
    private final HubToolbarCoordinator mHubToolbarCoordinator;
    private final HubPaneHostCoordinator mHubPaneHostCoordinator;
    private final HubLayoutController mHubLayoutController;
    private final ObservableSupplierImpl<Boolean> mHandleBackPressSupplier;
    private final HubSearchBoxBackgroundCoordinator mHubSearchBoxBackgroundCoordinator;

    /**
     * Generic callback that invokes {@link #updateHandleBackPressSupplier()}. This can be cast to
     * an arbitrary {@link Callback} and the provided value is discarded.
     */
    private final Callback<Object> mBackPressStateChangeCallback;

    /**
     * Warning: {@link #getFocusedPane()} may return null if no pane is focused or {@link
     * Pane#getHandleBackPressChangedSupplier()} contains null.
     */
    private final TransitiveObservableSupplier<Pane, Boolean> mFocusedPaneHandleBackPressSupplier;

    private final PaneBackStackHandler mPaneBackStackHandler;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

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
     * @param hubColorMixer Mixes the Hub Overview Color.
     */
    public HubCoordinator(
            Activity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            FrameLayout containerView,
            PaneManager paneManager,
            HubLayoutController hubLayoutController,
            ObservableSupplier<Tab> currentTabSupplier,
            MenuButtonCoordinator menuButtonCoordinator,
            SearchActivityClient searchActivityClient,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            HubColorMixer hubColorMixer) {
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
        int layoutId = XrUtils.isXrDevice() ? R.layout.hub_xr_layout : R.layout.hub_layout;
        mMainHubParent = LayoutInflater.from(context).inflate(layoutId, null);
        mContainerView.addView(mMainHubParent);

        ProfileProvider profileProvider = profileProviderSupplier.get();
        assert profileProvider != null;
        Profile profile = profileProvider.getOriginalProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        HubToolbarView hubToolbarView = mContainerView.findViewById(R.id.hub_toolbar);
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profile, new Handler());
        mHubToolbarCoordinator =
                new HubToolbarCoordinator(
                        activity,
                        hubToolbarView,
                        paneManager,
                        menuButtonCoordinator,
                        tracker,
                        searchActivityClient,
                        hubColorMixer,
                        userEducationHelper,
                        hubLayoutController.getIsAnimatingSupplier());

        HubPaneHostView hubPaneHostView = mContainerView.findViewById(R.id.hub_pane_host);
        mHubPaneHostCoordinator =
                new HubPaneHostCoordinator(
                        hubPaneHostView, paneManager.getFocusedPaneSupplier(), hubColorMixer);

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

        mHubSearchBoxBackgroundCoordinator = new HubSearchBoxBackgroundCoordinator(mContainerView);
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

        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        if (Boolean.TRUE.equals(mFocusedPaneHandleBackPressSupplier.get())
                && assumeNonNull(getFocusedPane()).handleBackPress() == BackPressResult.SUCCESS) {
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

    @Nullable
    @Override
    public Boolean handleEscPress() {
        if (Boolean.TRUE.equals(mFocusedPaneHandleBackPressSupplier.get())
                && assumeNonNull(getFocusedPane()).handleBackPress() == BackPressResult.SUCCESS) {
            return Boolean.TRUE;
        }

        Tab tab = mCurrentTabSupplier.get();
        if (tab != null) {
            mHubLayoutController.selectTabAndHideHubLayout(tab.getId());
            return Boolean.TRUE;
        }
        return Boolean.FALSE;
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        // We want a slightly different flow for Escape presses. Escape will close dialogs, and
        // close the Hub, but will not navigate back in Hub pane history like Back presses.
        return false;
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

    @Override
    public void setSearchBoxBackgroundProperties(boolean shouldShow) {
        // Early exit if the search box is not active like in phone landscape or tablets.
        if (!mHubToolbarCoordinator.isSearchBoxVisible()) return;
        mHubSearchBoxBackgroundCoordinator.setShouldShowBackground(shouldShow);
        mHubSearchBoxBackgroundCoordinator.setBackgroundColorScheme(
                assumeNonNull(getFocusedPane()).getColorScheme());
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
