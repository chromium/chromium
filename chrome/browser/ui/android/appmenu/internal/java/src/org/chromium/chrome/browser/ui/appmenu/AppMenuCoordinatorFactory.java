// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.WindowAndroid;

/** A factory for creating an {@link AppMenuCoordinator}. */
public class AppMenuCoordinatorFactory {
    private AppMenuCoordinatorFactory() {}

    /**
     * Create a new AppMenuCoordinator.
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
    public static AppMenuCoordinator createAppMenuCoordinator(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuButtonDelegate buttonDelegate,
            AppMenuDelegate appMenuDelegate,
            View decorView,
            View hardwareButtonAnchorView,
            Supplier<Rect> appRect,
            WindowAndroid windowAndroid) {
        return new AppMenuCoordinatorImpl(
                context,
                activityLifecycleDispatcher,
                buttonDelegate,
                appMenuDelegate,
                decorView,
                hardwareButtonAnchorView,
                appRect,
                windowAndroid);
    }

    /**
     * @param reporter A means of reporting an exception without crashing.
     */
    public static void setExceptionReporter(Callback<Throwable> reporter) {
        AppMenuCoordinatorImpl.setExceptionReporter(reporter);
    }
}
