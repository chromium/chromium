// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Widget that provides an entry point for users to quickly perform actions in Chrome.
 */
public class QuickActionSearchWidgetProvider extends AppWidgetProvider {
    /**
     * This function initializes the QuickActionSearchWidgetProvider component.
     * Namely, this function enables the component for users who have the
     * QUICK_ACTION_SEARCH_WIDGET flag enabled.
     *
     * Note that due to b/189087746, the widget cannot be disabled be default,
     * as a result, we must enable/disable the widget programmatically here.
     *
     * This function is called exactly once after native libraries are initialized.
     */
    public static void initialize() {
        boolean shouldEnableQuickActionSearchWidget =
                ChromeFeatureList.isEnabled(ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET);

        int componentEnabledState = shouldEnableQuickActionSearchWidget
                ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        Context context = ContextUtils.getApplicationContext();
        ComponentName componentName =
                new ComponentName(context, QuickActionSearchWidgetProvider.class);

        context.getPackageManager().setComponentEnabledSetting(
                componentName, componentEnabledState, PackageManager.DONT_KILL_APP);
    }
}
