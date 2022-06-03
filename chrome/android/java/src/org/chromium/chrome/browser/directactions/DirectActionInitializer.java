// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.content.Context;
import android.os.Bundle;
import android.os.CancellationSignal;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.Collections;
import java.util.List;
import java.util.function.Consumer;

/**
 * A wrapper for initializing {@link DirectActionCoordinator} with standard direct actions from
 * Chrome activities.
 */
@RequiresApi(29)
public class DirectActionInitializer implements NativeInitObserver, DestroyObserver {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final CompositorViewHolder mCompositorViewHolder;
    private final ActivityTabProvider mActivityTabProvider;
    private final TabModelSelector mTabModelSelector;

    @ActivityType
    private int mActivityType;
    private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private Runnable mGoBackAction;
    private boolean mDirectActionsRegistered;

    /**
     * @param context The current context, often the activity instance.
     * @param activityType The type of the current activity
     * @param actionController Controller to use to execute menu actions
     * @param goBackAction Implementation of the "go_back" action, usually {@link
     *         android.app.Activity#onBackPressed}.
     * @param tabModelSelector The activity's {@link TabModelSelector}
     * @param bottomSheetController Controller for the activity's bottom sheet, if it exists
     * @param browserControls Provider of browser controls of the activity
     * @param compositorViewHolder Compositor view holder of the activity
     * @param activityTabProvider Activity tab provider
     */
    public DirectActionInitializer(Context context, @ActivityType int activityType,
            MenuOrKeyboardActionController actionController, Runnable goBackAction,
            TabModelSelector tabModelSelector, @Nullable BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            ActivityTabProvider activityTabProvider) {
        mContext = context;
        mActivityType = activityType;
        mMenuOrKeyboardActionController = actionController;
        mGoBackAction = goBackAction;
        mTabModelSelector = tabModelSelector;
        mBottomSheetController = bottomSheetController;
        mBrowserControls = browserControls;
        mCompositorViewHolder = compositorViewHolder;
        mActivityTabProvider = activityTabProvider;

        mDirectActionsRegistered = false;
    }

    /**
     * Performs a direct action.
     *
     * @param actionId Name of the direct action to perform.
     * @param arguments Arguments for this action.
     * @param cancellationSignal Signal used to cancel a direct action from the caller.
     * @param callback Callback to run when the action is done.
     */
    public void onPerformDirectAction(String actionId, Bundle arguments,
            CancellationSignal cancellationSignal, Consumer<Bundle> callback) {
        callback.accept(Bundle.EMPTY);
    }

    /**
     * Lists direct actions supported.
     *
     * Returns a list of direct actions supported by the Activity associated with this
     * RootUiCoordinator.
     *
     * @param cancellationSignal Signal used to cancel a direct action from the caller.
     * @param callback Callback to run when the action is done.
     */
    public void onGetDirectActions(CancellationSignal cancellationSignal, Consumer<List> callback) {
        callback.accept(Collections.emptyList());
    }

    /**
     * Allows a specific set of menu actions in addition to the common actions.
     *
     * @param actionController Controller to use to execute menu action
     * @param tabModelSelector The activity's {@link TabModelSelector}
     */
    void allowMenuActions(MenuOrKeyboardActionController actionController,
            TabModelSelector tabModelSelector, Integer... itemIds) {
    }

    // Implements DestroyObserver
    @Override
    public void onDestroy() {
        mDirectActionsRegistered = false;
    }

    // Implements NativeInitObserver
    @Override
    public void onFinishNativeInitialization() {
    }

}
