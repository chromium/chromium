// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.WindowAndroid;

/** A UI coordinator the app menu. */
class AppMenuCoordinatorImpl implements AppMenuCoordinator {
    private static Boolean sHasPermanentMenuKeyForTesting;

    /** Factory which creates the AppMenuHandlerImpl. */
    @VisibleForTesting
    interface AppMenuHandlerFactory {
        /**
         * @param delegate Delegate used to check the desired AppMenu properties on show.
         * @param appMenuDelegate The AppMenuDelegate to handle menu item selection.
         * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the
         *         containing activity.
         * @param menuResourceId Resource Id that should be used as the source for the menu items.
         *            It is assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
         * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the
         *         containing activity.
         * @return AppMenuHandlerImpl for the given activity and menu resource id.
         */
        AppMenuHandlerImpl get(
                AppMenuPropertiesDelegate delegate,
                AppMenuDelegate appMenuDelegate,
                int menuResourceId,
                View decorView,
                ActivityLifecycleDispatcher activityLifecycleDispatcher);
    }

    private final Context mContext;
    private final MenuButtonDelegate mButtonDelegate;
    private final AppMenuDelegate mAppMenuDelegate;

    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuHandlerImpl mAppMenuHandler;

    /**
     * Construct a new AppMenuCoordinatorImpl.
     * @param context The activity context.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *     activity.
     * @param buttonDelegate The {@link MenuButtonDelegate} for the containing activity.
     * @param appMenuDelegate The {@link AppMenuDelegate} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *     activity.
     * @param hardwareButtonAnchorView The {@link View} used as an anchor for the menu when it is
     *     displayed using a hardware button.
     * @param appRect Supplier of the app area in Window that the menu should fit in.
     * @param windowAndroid The window that will be used to fetch KeyboardVisibilityDelegate
     */
    public AppMenuCoordinatorImpl(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuButtonDelegate buttonDelegate,
            AppMenuDelegate appMenuDelegate,
            View decorView,
            View hardwareButtonAnchorView,
            Supplier<Rect> appRect,
            WindowAndroid windowAndroid) {
        mContext = context;
        mButtonDelegate = buttonDelegate;
        mAppMenuDelegate = appMenuDelegate;
        mAppMenuPropertiesDelegate = mAppMenuDelegate.createAppMenuPropertiesDelegate();

        mAppMenuHandler =
                new AppMenuHandlerImpl(
                        mContext,
                        mAppMenuPropertiesDelegate,
                        mAppMenuDelegate,
                        decorView,
                        activityLifecycleDispatcher,
                        hardwareButtonAnchorView,
                        appRect,
                        windowAndroid);
    }

    @Override
    public void destroy() {
        // Prevent the menu window from leaking.
        if (mAppMenuHandler != null) mAppMenuHandler.destroy();

        mAppMenuPropertiesDelegate.destroy();
    }

    @Override
    public void showAppMenuForKeyboardEvent() {
        if (mAppMenuHandler == null || !mAppMenuHandler.shouldShowAppMenu()) return;

        boolean hasPermanentMenuKey =
                sHasPermanentMenuKeyForTesting != null
                        ? sHasPermanentMenuKeyForTesting.booleanValue()
                        : ViewConfiguration.get(mContext).hasPermanentMenuKey();
        mAppMenuHandler.showAppMenu(
                hasPermanentMenuKey ? null : mButtonDelegate.getMenuButtonView(), false);
    }

    @Override
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
    }

    @Override
    public AppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return mAppMenuPropertiesDelegate;
    }

    @Override
    public void registerAppMenuBlocker(AppMenuBlocker blocker) {
        mAppMenuHandler.registerAppMenuBlocker(blocker);
    }

    @Override
    public void unregisterAppMenuBlocker(AppMenuBlocker blocker) {
        mAppMenuHandler.unregisterAppMenuBlocker(blocker);
    }

    // Testing methods

    AppMenuHandlerImpl getAppMenuHandlerImplForTesting() {
        return mAppMenuHandler;
    }

    /**
     * @param hasPermanentMenuKey Overrides {@link ViewConfiguration#hasPermanentMenuKey()} for
     *         testing. Pass null to reset.
     */
    static void setHasPermanentMenuKeyForTesting(Boolean hasPermanentMenuKey) {
        sHasPermanentMenuKeyForTesting = hasPermanentMenuKey;
        ResettersForTesting.register(() -> sHasPermanentMenuKeyForTesting = null);
    }

    /** @param reporter A means of reporting an exception without crashing. */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        AppMenuHandlerImpl.setExceptionReporter(reporter);
    }
}
