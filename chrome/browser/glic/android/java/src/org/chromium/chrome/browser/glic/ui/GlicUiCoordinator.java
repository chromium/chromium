// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.app.Activity;
import android.view.ViewStub;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTaskHelper;
import org.chromium.chrome.browser.actor.notifications.ActorExternalTriggerSnackbarController;
import org.chromium.chrome.browser.actor.ui.ActorOverlayCoordinator;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.ui.base.WindowAndroid;

/**
 * Coordinator for Glic UI. It owns and manages Glic-related UI components, currently including
 * ActorControlCoordinator, ActorOverlayCoordinator, ActorTaskHelper, and
 * ActorExternalTriggerSnackbarController.
 */
@NullMarked
public class GlicUiCoordinator implements Destroyable {

    private static final UnownedUserDataKey<GlicUiCoordinator> KEY = new UnownedUserDataKey<>();

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ActorOverlayCoordinator mActorOverlayCoordinator;
    private final ActorTaskHelper mActorTaskHelper;
    private final ActorExternalTriggerSnackbarController mExternalTriggerSnackbarController;

    /**
     * Constructs a new {@link GlicUiCoordinator}.
     *
     * @param activity The {@link Activity} hosting the UI.
     * @param windowAndroid The {@link WindowAndroid} for the current window.
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for managing bottom sheets.
     * @param profileSupplier Supplier for the current {@link Profile}.
     * @param activityTabProvider Supplier for the active {@link Tab}.
     * @param tabModelSelectorSupplier Supplier for the {@link TabModelSelector}.
     * @param browserControlsVisibilityManager Manager for browser controls visibility.
     * @param tabObscuringHandler Handler for obscuring tabs.
     * @param snackbarManager Manager for showing snackbars.
     * @param backPressHandlerRegistry Registry for back press handlers.
     * @param layoutManagerSupplier Supplier for the {@link LayoutManager}.
     * @param overlayStub The {@link ViewStub} to inflate the overlay into.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param omniboxFocusStateSupplier Supplier for whether the omnibox currently has focus.
     * @param sideUiStateProvider The {@link SideUiStateProvider} providing state on the side UI.
     */
    public GlicUiCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            @Nullable TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> activityTabProvider,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler,
            SnackbarManager snackbarManager,
            BackPressHandlerRegistry backPressHandlerRegistry,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier,
            ViewStub overlayStub,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @Nullable SideUiStateProvider sideUiStateProvider) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;

        mActorOverlayCoordinator =
                new ActorOverlayCoordinator(
                        overlayStub,
                        tabModelSelectorSupplier.asNonNull().get(),
                        browserControlsVisibilityManager,
                        tabObscuringHandler,
                        snackbarManager,
                        backPressHandlerRegistry,
                        layoutManagerSupplier,
                        profileSupplier,
                        omniboxFocusStateSupplier,
                        sideUiStateProvider);

        mActorTaskHelper =
                new ActorTaskHelper(
                        mActivity,
                        profileSupplier,
                        tabModelSelectorSupplier,
                        activityLifecycleDispatcher);

        mExternalTriggerSnackbarController =
                new ActorExternalTriggerSnackbarController(
                        mActivity, snackbarManager, profileSupplier, activityLifecycleDispatcher);

        KEY.attachToHost(mWindowAndroid.getUnownedUserDataHost(), this);
    }

    /** Returns the {@link GlicUiCoordinator} associated with the given {@link WindowAndroid}. */
    static @Nullable GlicUiCoordinator from(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    @Override
    public void destroy() {
        KEY.detachFromHost(mWindowAndroid.getUnownedUserDataHost());
        mExternalTriggerSnackbarController.destroy();
        mActorOverlayCoordinator.destroy();
        mActorTaskHelper.destroy();
    }

    /**
     * Notifies the snackbar controller that a pending actor task trigger was received (either from
     * an external-trigger fallback intent or when showing/accepting the experimental opt-in
     * dialog).
     */
    public void onPendingActorTaskTrigger() {
        mExternalTriggerSnackbarController.onPendingActorTaskTrigger();
    }

    /**
     * Notifies the snackbar controller that a pending actor task trigger failed before task
     * creation (e.g., when the experimental opt-in dialog is declined or dismissed).
     */
    public void onPendingActorTaskFailed() {
        mExternalTriggerSnackbarController.onPendingActorTaskFailed();
    }

    /** Exposes the {@link ActorOverlayCoordinator} for testing. */
    public ActorOverlayCoordinator getActorOverlayCoordinatorForTesting() {
        return mActorOverlayCoordinator;
    }
}
