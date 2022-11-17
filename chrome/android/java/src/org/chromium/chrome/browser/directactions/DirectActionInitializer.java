// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;
import android.os.CancellationSignal;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.Collections;
import java.util.List;
import java.util.function.Consumer;

/**
 * A wrapper for initializing {@link DirectActionCoordinator} with standard direct actions from
 * Chrome activities.
 *
 * <p>To extend the set of direct actions beyond what's provided by this class, register handlers to
 * the coordinator {@code mCoordinator}.
 */
@RequiresApi(29)
public class DirectActionInitializer implements NativeInitObserver, DestroyObserver {
    private final TabModelSelector mTabModelSelector;

    @ActivityType
    private int mActivityType;
    private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private Runnable mGoBackAction;
    @Nullable
    private FindToolbarManager mFindToolbarManager;
    private boolean mDirectActionsRegistered;
    @Nullable
    private DirectActionCoordinator mCoordinator;
    @Nullable
    private MenuDirectActionHandler mMenuHandler;

    /**
     * @param activityType The type of the current activity
     * @param actionController Controller to use to execute menu actions
     * @param goBackAction Implementation of the "go_back" action, usually {@link
     *         android.app.Activity#onBackPressed}.
     * @param tabModelSelector The activity's {@link TabModelSelector}
     * @param findToolbarManager Manager to use for the "find_in_page" action, if it exists
     */
    public DirectActionInitializer(@ActivityType int activityType,
            MenuOrKeyboardActionController actionController, Runnable goBackAction,
            TabModelSelector tabModelSelector, @Nullable FindToolbarManager findToolbarManager) {
        mActivityType = activityType;
        mMenuOrKeyboardActionController = actionController;
        mGoBackAction = goBackAction;
        mTabModelSelector = tabModelSelector;
        mFindToolbarManager = findToolbarManager;

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
        if (mCoordinator == null || !mDirectActionsRegistered) {
            callback.accept(Bundle.EMPTY);
            return;
        }
        mCoordinator.onPerformDirectAction(actionId, arguments, callback);
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
        if (mCoordinator == null || !mDirectActionsRegistered) {
            callback.accept(Collections.emptyList());
            return;
        }
        mCoordinator.onGetDirectActions(callback);
    }

    /**
     * Registers common action that manipulate the current activity or the browser content.
     *
     * @param actionController Controller to use to execute menu actions
     * @param goBackAction Implementation of the "go_back" action, usually {@link
     *         android.app.Activity#onBackPressed}.
     * @param tabModelSelector The activity's {@link TabModelSelector}
     * @param findToolbarManager Manager to use for the "find_in_page" action, if it exists
     */
    private void registerCommonChromeActions(MenuOrKeyboardActionController actionController,
            Runnable goBackAction, TabModelSelector tabModelSelector,
            @Nullable FindToolbarManager findToolbarManager) {
        mCoordinator.register(new GoBackDirectActionHandler(goBackAction));
        mCoordinator.register(
                new FindInPageDirectActionHandler(tabModelSelector, findToolbarManager));

        registerMenuHandlerIfNecessary(actionController, tabModelSelector)
                .allowlistActions(R.id.forward_menu_id, R.id.reload_menu_id);
    }

    /**
     * Registers actions that manipulate tabs in addition to the common actions.
     *
     * @param actionController Controller to use to execute menu action
     * @param tabModelSelector The activity's {@link TabModelSelector}
     */
    void registerTabManipulationActions(
            MenuOrKeyboardActionController actionController, TabModelSelector tabModelSelector) {
        registerMenuHandlerIfNecessary(actionController, tabModelSelector).allowAllActions();
        mCoordinator.register(new CloseTabDirectActionHandler(tabModelSelector));
    }

    /**
     * Allows a specific set of menu actions in addition to the common actions.
     *
     * @param actionController Controller to use to execute menu action
     * @param tabModelSelector The activity's {@link TabModelSelector}
     */
    void allowMenuActions(MenuOrKeyboardActionController actionController,
            TabModelSelector tabModelSelector, Integer... itemIds) {
        registerMenuHandlerIfNecessary(actionController, tabModelSelector)
                .allowlistActions(itemIds);
    }

    // Implements DestroyObserver
    @Override
    public void onDestroy() {
        mCoordinator = null;
        mDirectActionsRegistered = false;
    }

    // Implements NativeInitObserver
    @Override
    public void onFinishNativeInitialization() {
        mCoordinator = AppHooks.get().createDirectActionCoordinator();
        if (mCoordinator != null) {
            mCoordinator.init(/* isEnabled= */ () -> !mTabModelSelector.isIncognitoSelected());
            registerDirectActions();
        }
    }

    /**
     * Registers the set of direct actions available to assist apps.
     */
    void registerDirectActions() {
        registerCommonChromeActions(mMenuOrKeyboardActionController, mGoBackAction,
                mTabModelSelector, mFindToolbarManager);

        if (mActivityType == ActivityType.TABBED) {
            registerTabManipulationActions(mMenuOrKeyboardActionController, mTabModelSelector);
        } else if (mActivityType == ActivityType.CUSTOM_TAB) {
            allowMenuActions(mMenuOrKeyboardActionController, mTabModelSelector,
                    R.id.bookmark_this_page_id, R.id.preferences_id);
        }

        mDirectActionsRegistered = true;
    }

    private MenuDirectActionHandler registerMenuHandlerIfNecessary(
            MenuOrKeyboardActionController actionController, TabModelSelector tabModelSelector) {
        if (mMenuHandler == null) {
            mMenuHandler = new MenuDirectActionHandler(actionController, tabModelSelector);
            mCoordinator.register(mMenuHandler);
        }
        return mMenuHandler;
    }
}
