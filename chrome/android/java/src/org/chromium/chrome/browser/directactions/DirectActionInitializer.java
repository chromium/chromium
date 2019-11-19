// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.annotation.TargetApi;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity.ActivityType;
import org.chromium.chrome.browser.MenuOrKeyboardActionController;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * A helper for initializing {@link DirectActionCoordinator} with standard direct actions from
 * Chrome activities.
 *
 * <p>To extend the set of direct actions beyond what's provided by this class, register handlers to
 * the coordinator returned by {@link #getCoordinator}.
 */
@TargetApi(29)
public class DirectActionInitializer {
    @NonNull
    private DirectActionCoordinator mCoordinator;

    @Nullable
    private MenuDirectActionHandler mMenuHandler;

    /**
     * Creates and initializes a {@link DirectActionHandler}, if possible, for a specific activity.
     *
     * @param tabModelSelector The activity's {@link TabModelSelector}.
     */
    @Nullable
    public static DirectActionInitializer create(TabModelSelector tabModelSelector) {
        DirectActionCoordinator coordinator = AppHooks.get().createDirectActionCoordinator();
        if (coordinator == null) return null;

        coordinator.init(/* isEnabled= */ () -> !tabModelSelector.isIncognitoSelected());
        return new DirectActionInitializer(coordinator);
    }

    /** Returns the coordinator. */
    public DirectActionCoordinator getCoordinator() {
        return mCoordinator;
    }

    private DirectActionInitializer(DirectActionCoordinator coordinator) {
        mCoordinator = coordinator;
    }

    /**
     * Registers common action that manipulate the current activity or the browser content.
     *
     * @param context The current context, often the activity instance.
     * @param activityType The type of the current activity
     * @param actionController Controller to use to execute menu actions
     * @param goBackAction Implementation of the "go_back" action, usually {@link
     *         android.app.Activity#onBackPressed}.
     * @param tabModelSelector The activity's {@link TabModelSelector}
     * @param bottomSheetController Controller for the activity's bottom sheet, if it exists
     * @param scrim The activity's scrim view, if it exists
     */
    public void registerCommonChromeActions(Context context, @ActivityType int activityType,
            MenuOrKeyboardActionController actionController, Runnable goBackAction,
            TabModelSelector tabModelSelector,
            @Nullable BottomSheetController bottomSheetController, ScrimView scrim) {
        mCoordinator.register(new GoBackDirectActionHandler(goBackAction));

        registerMenuHandlerIfNecessary(actionController, tabModelSelector)
                .whitelistActions(R.id.forward_menu_id, R.id.reload_menu_id);

        if (AutofillAssistantFacade.areDirectActionsAvailable(activityType)) {
            DirectActionHandler handler = AutofillAssistantFacade.createDirectActionHandler(
                    context, bottomSheetController, scrim, tabModelSelector);
            if (handler != null) mCoordinator.register(handler);
        }
    }

    /**
     * Registers actions that manipulate tabs in addition to the common actions.
     *
     * @param actionController Controller to use to execute menu action
     * @param tabModelSelector The activity's {@link TabModelSelector}
     */
    public void registerTabManipulationActions(
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
    public void allowMenuActions(MenuOrKeyboardActionController actionController,
            TabModelSelector tabModelSelector, Integer... itemIds) {
        registerMenuHandlerIfNecessary(actionController, tabModelSelector)
                .whitelistActions(itemIds);
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
