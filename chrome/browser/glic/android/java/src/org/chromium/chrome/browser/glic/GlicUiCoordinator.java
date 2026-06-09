// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.view.ViewStub;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTaskHelper;
import org.chromium.chrome.browser.actor.ui.ActorControlCoordinator;
import org.chromium.chrome.browser.actor.ui.ActorControlStateTracker;
import org.chromium.chrome.browser.actor.ui.ActorOverlayCoordinator;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;

/**
 * Coordinator for Glic UI. It owns and manages Glic-related UI components, currently including
 * ActorControlCoordinator, ActorOverlayCoordinator, and ActorTaskHelper.
 */
@NullMarked
public class GlicUiCoordinator implements Destroyable {

    private final Activity mActivity;
    private final ActorControlStateTracker mActorControlStateTracker;
    private final ActorControlCoordinator mActorControlCoordinator;
    private final ActorOverlayCoordinator mActorOverlayCoordinator;
    private final ActorTaskHelper mActorTaskHelper;

    /**
     * Constructs a new {@link GlicUiCoordinator}.
     *
     * @param activity The {@link Activity} hosting the UI.
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
     * @param bottomSheetController Controller for bottom sheets.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param sideUiStateProvider The {@link SideUiStateProvider} providing state on the side UI.
     */
    public GlicUiCoordinator(
            Activity activity,
            TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> activityTabProvider,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler,
            SnackbarManager snackbarManager,
            BackPressHandlerRegistry backPressHandlerRegistry,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier,
            ViewStub overlayStub,
            BottomSheetController bottomSheetController,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @Nullable SideUiStateProvider sideUiStateProvider) {
        mActivity = activity;

        mActorControlStateTracker =
                new ActorControlStateTracker(profileSupplier, activityTabProvider);

        mActorControlCoordinator =
                new ActorControlCoordinator(
                        tabBottomSheetManager,
                        mActorControlStateTracker,
                        (tabId) -> {
                            TabModelSelector selector = tabModelSelectorSupplier.get();
                            if (selector != null) {
                                TabModelUtils.selectTabById(
                                        selector, tabId, TabSelectionType.FROM_USER);
                            }
                        });

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
                        bottomSheetController,
                        sideUiStateProvider);

        mActorTaskHelper =
                new ActorTaskHelper(
                        mActivity,
                        profileSupplier,
                        tabModelSelectorSupplier,
                        activityLifecycleDispatcher);
    }

    @Override
    public void destroy() {
        mActorControlStateTracker.destroy();
        mActorControlCoordinator.destroy();
        mActorOverlayCoordinator.destroy();
        mActorTaskHelper.onDestroy();
        mActorTaskHelper.destroy();
    }

    /** Exposes the {@link ActorOverlayCoordinator} for testing. */
    public ActorOverlayCoordinator getActorOverlayCoordinatorForTesting() {
        return mActorOverlayCoordinator;
    }
}
