// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;

/**
 * {@link AppWidgetProvider} for a widget that provides an entry point for users to quickly perform
 * actions in Chrome.
 */
public class QuickActionSearchWidgetProvider extends AppWidgetProvider {
    private QuickActionSearchWidgetProviderDelegate mDelegate;

    @Override
    public void onUpdate(
            final Context context, final AppWidgetManager manager, final int[] widgetIds) {
        getDelegate(context).updateWidgets(context, manager, widgetIds);
    }

    /**
     * This function lazily initializes and returns the
     * {@link QuickActionSearchWidgetProviderDelegate}
     * for this instance.
     * <p>
     * We don't initialize the delegate in the constructor because creation of the
     * QuickActionSearchWidgetProvider is done by the system.
     */
    private QuickActionSearchWidgetProviderDelegate getDelegate(final Context context) {
        if (mDelegate == null) {
            ComponentName widgetReceiverComponent =
                    new ComponentName(context, QuickActionSearchWidgetReceiver.class);

            mDelegate = new QuickActionSearchWidgetProviderDelegate(widgetReceiverComponent);
        }
        return mDelegate;
    }

    /**
     * This function initializes the QuickActionSearchWidgetProvider component. Namely, this
     * function enables the component for users who have the QUICK_ACTION_SEARCH_WIDGET flag
     * enabled.
     * <p>
     * Note that due to b/189087746, the widget cannot be disabled be default, as a result, we must
     * enable/disable the widget programmatically here.
     * <p>
     * This function is expected to be called exactly once after native libraries are initialized.
     */
    public static void initialize() {
        setWidgetEnabled(ChromeFeatureList.isEnabled(ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET));
    }

    /**
     * Enables/Disables the widget component. If the widget is disabled, it will not appear in the
     * widget picker, and users will not be able to add the widget.
     *
     * @param shouldEnableQuickActionSearchWidget a boolean indicating whether the widget component
     *                                            should be enabled or not.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void setWidgetEnabled(boolean shouldEnableQuickActionSearchWidget) {
        int componentEnabledState = shouldEnableQuickActionSearchWidget
                ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        Context context = ContextUtils.getApplicationContext();
        ComponentName componentName =
                new ComponentName(context, QuickActionSearchWidgetProvider.class);

        context.getPackageManager().setComponentEnabledSetting(
                componentName, componentEnabledState, PackageManager.DONT_KILL_APP);
    }

    /** Sets a QuickActionSearchWidgetProviderDelegate to facilitate tests. */
    @VisibleForTesting
    void setDelegateForTesting(QuickActionSearchWidgetProviderDelegate delegate) {
        mDelegate = delegate;
    }
}
