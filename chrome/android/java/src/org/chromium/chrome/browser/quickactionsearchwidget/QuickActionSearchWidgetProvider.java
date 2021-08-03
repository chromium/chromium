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
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetType;

/**
 * {@link AppWidgetProvider} for a widget that provides an entry point for users to quickly perform
 * actions in Chrome.
 */
public abstract class QuickActionSearchWidgetProvider extends AppWidgetProvider {
    private QuickActionSearchWidgetProviderDelegate mDelegate;

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * initially has the small layout.
     */
    public static class QuickActionSearchWidgetProviderSmall
            extends QuickActionSearchWidgetProvider {
        @Override
        protected int getWidgetType() {
            return QuickActionSearchWidgetType.SMALL;
        }
    }

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * initially has the medium layout.
     */
    public static class QuickActionSearchWidgetProviderMedium
            extends QuickActionSearchWidgetProvider {
        @Override
        protected int getWidgetType() {
            return QuickActionSearchWidgetType.MEDIUM;
        }
    }

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * only contains a touch surface for launching the Dino game.
     */
    public static class QuickActionSearchWidgetProviderDino
            extends QuickActionSearchWidgetProvider {
        @Override
        protected int getWidgetType() {
            return QuickActionSearchWidgetType.DINO;
        }
    }

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
            int widgetType = getWidgetType();

            ComponentName widgetReceiverComponent =
                    new ComponentName(context, QuickActionSearchWidgetReceiver.class);

            mDelegate = new QuickActionSearchWidgetProviderDelegate(widgetType,
                    widgetReceiverComponent,
                    IntentHandler.createTrustedOpenNewTabIntent(context, /*incognito=*/true));
        }
        return mDelegate;
    }

    /** @return The {@link QuickActionSearchWidgetType} for this widget */
    protected abstract @QuickActionSearchWidgetType int getWidgetType();

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
        setWidgetEnabled(ChromeFeatureList.isEnabled(ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET),
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET_DINO_VARIANT));
    }

    /**
     * Enables/Disables the widget component. If the widget is disabled, it will not appear in the
     * widget picker, and users will not be able to add the widget.
     *
     * @param shouldEnableQuickActionSearchWidget a boolean indicating whether the widget component
     *                                            should be enabled or not.
     * @param shouldEnableDinoVariant a boolean indicating whether the widget component of the Dino
     *         variant should be enabled.
     */
    private static void setWidgetEnabled(
            boolean shouldEnableQuickActionSearchWidget, boolean shouldEnableDinoVariant) {
        setWidgetComponentEnabled(
                QuickActionSearchWidgetProviderSmall.class, shouldEnableQuickActionSearchWidget);
        setWidgetComponentEnabled(
                QuickActionSearchWidgetProviderMedium.class, shouldEnableQuickActionSearchWidget);
        setWidgetComponentEnabled(
                QuickActionSearchWidgetProviderDino.class, shouldEnableDinoVariant);
    }

    /**
     * Enables/Disables the given widget component for a variation of the Quick Action Search
     * Widget.
     *
     * @param component The {@link QuickActionSearchWidgetProvider} subclass corresponding to the
     *         widget that is to be disabled.
     * @param shouldEnableWidgetComponent a boolean indicating whether the widget component should
     *         be enabled or not.
     */
    private static void setWidgetComponentEnabled(
            final Class<? extends QuickActionSearchWidgetProvider> component,
            final boolean shouldEnableWidgetComponent) {
        // The initialization must be performed on a background thread because the following logic
        // can trigger disk access. The PostTask in ProcessInitializationHandler can be removed once
        // the experimentation phase is over.
        ThreadUtils.assertOnBackgroundThread();
        Context context = ContextUtils.getApplicationContext();

        int componentEnabledState = shouldEnableWidgetComponent
                ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        ComponentName componentName = new ComponentName(context, component);
        context.getPackageManager().setComponentEnabledSetting(
                componentName, componentEnabledState, PackageManager.DONT_KILL_APP);
    }

    /** Sets a QuickActionSearchWidgetProviderDelegate to facilitate tests. */
    @VisibleForTesting
    void setDelegateForTesting(QuickActionSearchWidgetProviderDelegate delegate) {
        mDelegate = delegate;
    }
}
