// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubToolbarMediator.INVALID_PANE_SWITCHER_INDEX;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.hub.HubPaneHostView.OnPaneSwipeListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

import java.util.List;

/** Root coordinator of the Hub. */
@NullMarked
public class HubCoordinator implements PaneHubController, BackPressHandler, OnPaneSwipeListener {
    private final FrameLayout mContainerView;
    private final ViewGroup mMainHubParent;
    private final PaneManager mPaneManager;
    private final HubToolbarCoordinator mHubToolbarCoordinator;
    private final @Nullable HubBottomToolbarCoordinator mHubBottomToolbarCoordinator;
    private final HubPaneHostCoordinator mHubPaneHostCoordinator;
    private final SingleChildViewManager mOverlayViewManager;
    private final HubLayoutController mHubLayoutController;
    private final SettableNonNullObservableSupplier<Boolean> mHandleBackPressSupplier =
            ObservableSuppliers.createNonNull(false);

    private final HubSearchBoxBackgroundCoordinator mHubSearchBoxBackgroundCoordinator;

    /**
     * Generic callback that invokes {@link #updateHandleBackPressSupplier()}. This can be cast to
     * an arbitrary {@link Callback} and the provided value is discarded.
     */
    private final Callback<@Nullable Object> mBackPressStateChangeCallback;

    /**
     * Warning: {@link #getFocusedPane()} may return null if no pane is focused or {@link
     * Pane#getHandleBackPressChangedSupplier()} contains null.
     */
    private final NonNullObservableSupplier<Boolean> mFocusedPaneHandleBackPressSupplier;

    private final PaneBackStackHandler mPaneBackStackHandler;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
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
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     * @param defaultPaneId The default pane's Id.
     */
    public HubCoordinator(
            Activity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            FrameLayout containerView,
            PaneManager paneManager,
            HubLayoutController hubLayoutController,
            NullableObservableSupplier<Tab> currentTabSupplier,
            MenuButtonCoordinator menuButtonCoordinator,
            SearchActivityClient searchActivityClient,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            HubColorMixer hubColorMixer,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            @PaneId int defaultPaneId) {
        Context context = containerView.getContext();
        mBackPressStateChangeCallback = (ignored) -> updateHandleBackPressSupplier();
        mPaneManager = paneManager;
        mFocusedPaneHandleBackPressSupplier =
                paneManager
                        .getFocusedPaneSupplier()
                        .createTransitiveNonNull(
                                false, BackPressHandler::getHandleBackPressChangedSupplier);
        mFocusedPaneHandleBackPressSupplier.addSyncObserverAndPostIfNonNull(
                castCallback(mBackPressStateChangeCallback));

        mContainerView = containerView;
        int layoutId = DeviceInfo.isXr() ? R.layout.hub_xr_layout : R.layout.hub_layout;
        mMainHubParent = (ViewGroup) LayoutInflater.from(context).inflate(layoutId, null);
        mContainerView.addView(mMainHubParent);

        ProfileProvider profileProvider = profileProviderSupplier.get();
        assert profileProvider != null;
        Profile profile = profileProvider.getOriginalProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        HubToolbarView hubToolbarView = mContainerView.findViewById(R.id.hub_toolbar);
        hubToolbarView.setXrSpaceModeObservableSupplier(xrSpaceModeObservableSupplier);

        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profile, new Handler());

        // Get bottom toolbar delegate and visibility supplier
        HubBottomToolbarDelegate bottomToolbarDelegate =
                HubBottomToolbarDelegateFactory.createDelegate();
        NonNullObservableSupplier<Boolean> bottomToolbarVisibilitySupplier =
                bottomToolbarDelegate != null
                        ? bottomToolbarDelegate.getBottomToolbarVisibilitySupplier()
                        : ObservableSuppliers.alwaysFalse();

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
                        hubLayoutController.getIsAnimatingSupplier(),
                        bottomToolbarVisibilitySupplier,
                        () -> {
                            RecordUserAction.record("Hub.BackButtonPressed");
                            selectCurrentTabAndHideHub();
                        });

        // Dynamically add bottom toolbar if delegate is available and enabled
        if (bottomToolbarDelegate != null && bottomToolbarDelegate.isBottomToolbarEnabled()) {
            ViewGroup mainContainer = mContainerView.findViewById(R.id.hub_pane_host_container);
            mHubBottomToolbarCoordinator =
                    new HubBottomToolbarCoordinator(
                            context,
                            mainContainer,
                            paneManager,
                            hubColorMixer,
                            bottomToolbarDelegate,
                            edgeToEdgeSupplier);
        } else {
            mHubBottomToolbarCoordinator = null;
        }

        HubPaneHostView hubPaneHostView = mContainerView.findViewById(R.id.hub_pane_host);
        hubPaneHostView.setXrSpaceModeObservableSupplier(xrSpaceModeObservableSupplier);
        hubPaneHostView.setOnPaneSwipeListener(this);

        mHubPaneHostCoordinator =
                new HubPaneHostCoordinator(
                        hubPaneHostView,
                        paneManager.getFocusedPaneSupplier(),
                        hubColorMixer,
                        defaultPaneId);

        NullableObservableSupplier<View> overlayViewSupplier =
                mPaneManager
                        .getFocusedPaneSupplier()
                        .createTransitiveNullable(Pane::getHubOverlayViewSupplier);
        mOverlayViewManager =
                new SingleChildViewManager(
                        mContainerView.findViewById(R.id.hub_overlay_container),
                        overlayViewSupplier);

        mHubLayoutController = hubLayoutController;

        mPaneBackStackHandler = new PaneBackStackHandler(paneManager);
        mPaneBackStackHandler
                .getHandleBackPressChangedSupplier()
                .addSyncObserverAndPostIfNonNull(castCallback(mBackPressStateChangeCallback));

        mCurrentTabSupplier = currentTabSupplier;
        setCurrentTabSupplierObserver();

        mHubLayoutController
                .getPreviousLayoutTypeSupplier()
                .addSyncObserverAndPostIfNonNull(castCallback(mBackPressStateChangeCallback));

        updateHandleBackPressSupplier();

        mHubSearchBoxBackgroundCoordinator = new HubSearchBoxBackgroundCoordinator(mContainerView);
    }

    @SuppressWarnings("NullAway")
    private void setCurrentTabSupplierObserver() {
        mCurrentTabSupplier.addSyncObserverAndPostIfNonNull(
                castCallback(mBackPressStateChangeCallback));
    }

    /** Removes the hub from the layout tree and cleans up resources. */
    @SuppressWarnings("NullAway")
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
        if (mHubBottomToolbarCoordinator != null) {
            mHubBottomToolbarCoordinator.destroy();
        }
        mHubPaneHostCoordinator.destroy();
        mOverlayViewManager.destroy();

        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        if (mFocusedPaneHandleBackPressSupplier.get()
                && assumeNonNull(getFocusedPane()).handleBackPress() == BackPressResult.SUCCESS) {
            return BackPressResult.SUCCESS;
        }

        if (mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                && mPaneBackStackHandler.handleBackPress() == BackPressResult.SUCCESS) {
            return BackPressResult.SUCCESS;
        }

        boolean success = selectCurrentTabAndHideHub();
        return success ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Nullable
    @Override
    public Boolean handleEscPress() {
        if (mFocusedPaneHandleBackPressSupplier.get()
                && assumeNonNull(getFocusedPane()).handleBackPress() == BackPressResult.SUCCESS) {
            return true;
        }

        return selectCurrentTabAndHideHub();
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        // We want a slightly different flow for Escape presses. Escape will close dialogs, and
        // close the Hub, but will not navigate back in Hub pane history like Back presses.
        return false;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
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

    @Override
    public void onPaneSwipe(boolean isSwipeLeft) {
        Pane currentPane = getFocusedPane();
        if (currentPane == null) return;

        RecordUserAction.record("Android.Hub.PaneSwiped");
        String direction = isSwipeLeft ? "Left" : "Right";
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Hub.PaneSwiped." + direction, currentPane.getPaneId(), PaneId.COUNT);

        List<Integer> orderedPaneIds =
                mPaneManager.getPaneOrderController().getPaneOrder().asList();
        int currentPaneIndex = orderedPaneIds.indexOf(currentPane.getPaneId());
        if (currentPaneIndex == INVALID_PANE_SWITCHER_INDEX) return;

        int nextPaneIndex =
                getAdjacentActivePaneIndex(currentPaneIndex, isSwipeLeft, orderedPaneIds);

        if (nextPaneIndex != INVALID_PANE_SWITCHER_INDEX) {
            @PaneId int nextPaneId = orderedPaneIds.get(nextPaneIndex);
            mPaneManager.focusPane(nextPaneId);
        }
    }

    private int getAdjacentActivePaneIndex(
            int currentPaneIndex, boolean isSwipeLeft, List<Integer> orderedPaneIds) {
        int paneCount = orderedPaneIds.size();
        if (paneCount <= 1) return INVALID_PANE_SWITCHER_INDEX;

        // Find the next available pane to switch to.
        for (int i = 1; i < paneCount; i++) {
            int nextPaneIndex;
            if (isSwipeLeft) {
                nextPaneIndex = (currentPaneIndex + i) % paneCount;
            } else {
                nextPaneIndex = (currentPaneIndex - i + paneCount) % paneCount;
            }

            @PaneId int nextPaneId = orderedPaneIds.get(nextPaneIndex);
            Pane pane = mPaneManager.getPaneForId(nextPaneId);
            if (pane != null && pane.getReferenceButtonDataSupplier().get() != null) {
                return nextPaneIndex;
            }
        }
        return INVALID_PANE_SWITCHER_INDEX;
    }

    private boolean selectCurrentTabAndHideHub() {
        Tab tab = mCurrentTabSupplier.get();
        if (tab != null) {
            mHubLayoutController.selectTabAndHideHubLayout(tab.getId());
            return true;
        }
        return false;
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
                mFocusedPaneHandleBackPressSupplier.get()
                        || mPaneBackStackHandler.getHandleBackPressChangedSupplier().get()
                        || (mCurrentTabSupplier.get() != null);
        mHandleBackPressSupplier.set(shouldHandleBackPress);
    }

    @SuppressWarnings("NullAway")
    private <T> Callback<T> castCallback(Callback callback) {
        return (Callback<T>) callback;
    }

    @Nullable HubBottomToolbarCoordinator getHubBottomToolbarCoordinatorForTesting() {
        return mHubBottomToolbarCoordinator;
    }
}
