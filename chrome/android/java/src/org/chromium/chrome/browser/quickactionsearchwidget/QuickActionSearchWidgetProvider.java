// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;

/**
 * {@link AppWidgetProvider} for a widget that provides an entry point for users to quickly perform
 * actions in Chrome.
 */
public abstract class QuickActionSearchWidgetProvider extends AppWidgetProvider {
    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * initially has the small layout.
     */
    public static class QuickActionSearchWidgetProviderSmall
            extends QuickActionSearchWidgetProvider {
        private static QuickActionSearchWidgetProviderDelegate sDelegate;

        @Override
        protected QuickActionSearchWidgetProviderDelegate getDelegate() {
            if (sDelegate == null) {
                sDelegate = createDelegate(R.layout.quick_action_search_widget_small_layout);
            }
            return sDelegate;
        }
    }

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * initially has the medium layout.
     */
    public static class QuickActionSearchWidgetProviderMedium
            extends QuickActionSearchWidgetProvider {
        private static QuickActionSearchWidgetProviderDelegate sDelegate;

        @Override
        protected QuickActionSearchWidgetProviderDelegate getDelegate() {
            if (sDelegate == null) {
                sDelegate = createDelegate(R.layout.quick_action_search_widget_medium_layout);
            }
            return sDelegate;
        }
    }

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * only contains a touch surface for launching the Dino game.
     */
    public static class QuickActionSearchWidgetProviderDino
            extends QuickActionSearchWidgetProvider {
        private static QuickActionSearchWidgetProviderDelegate sDelegate;

        @Override
        protected QuickActionSearchWidgetProviderDelegate getDelegate() {
            if (sDelegate == null) {
                sDelegate = createDelegate(R.layout.quick_action_search_widget_dino_layout);
            }
            return sDelegate;
        }
    }

    @Override
    public void onUpdate(@NonNull Context context, @NonNull AppWidgetManager manager,
            @Nullable int[] widgetIds) {
        updateWidgets(context, manager, SearchActivityPreferencesManager.getCurrent(), widgetIds);
    }

    /**
     * Apply update to widgets, reflecting feature availability on the widget surface.
     *
     * @param context Current context.
     * @param manager Widget manager.
     * @param preferences Search Activity preferences.
     * @param widgetIds List of Widget IDs that should be updated.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void updateWidgets(@NonNull Context context, @NonNull AppWidgetManager manager,
            @NonNull SearchActivityPreferences preferences, @NonNull int[] widgetIds) {
        if (widgetIds == null) {
            // Query all widgets associated with this component.
            widgetIds = manager.getAppWidgetIds(new ComponentName(context, getClass().getName()));
        }
        manager.updateAppWidget(
                widgetIds, getDelegate().createWidgetRemoteViews(context, preferences));
    }

    /**
     * Create a new QuickActionSearchWidgetProviderDelegate.
     *
     * This method should only be used when a new instance of the ProviderDelegate is needed.
     * In all other cases, use the getDelegate() method.
     *
     * @param widgetType The type of the Widget for which the ProviderDelegate should be built.
     */
    protected QuickActionSearchWidgetProviderDelegate createDelegate(@LayoutRes int layout) {
        Context context = ContextUtils.getApplicationContext();
        ComponentName widgetReceiverComponent =
                new ComponentName(context, QuickActionSearchWidgetReceiver.class);
        ComponentName searchActivityComponent = new ComponentName(context, SearchActivity.class);
        Intent trustedIncognitoIntent =
                IntentHandler.createTrustedOpenNewTabIntent(context, /*incognito=*/true);

        return new QuickActionSearchWidgetProviderDelegate(
                layout, widgetReceiverComponent, searchActivityComponent, trustedIncognitoIntent);
    }

    /**
     * This function lazily initializes and returns the
     * {@link QuickActionSearchWidgetProviderDelegate}
     * for this instance.
     * <p>
     * We don't initialize the delegate in the constructor because creation of the
     * QuickActionSearchWidgetProvider is done by the system.
     */
    protected abstract QuickActionSearchWidgetProviderDelegate getDelegate();

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
        PostTask.postTask(TaskTraits.BEST_EFFORT, () -> {
            // Changing the widget enabled state, which is only required during the experimentation
            // phase, can trigger disk access. This can be removed when the QuickActionSearchWidget
            // launches.
            setWidgetEnabled(
                    ChromeFeatureList.isEnabled(ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET),
                    ChromeFeatureList.isEnabled(
                            ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET_DINO_VARIANT));
        });

        QuickActionSearchWidgetProvider dinoWidget = new QuickActionSearchWidgetProviderDino();
        QuickActionSearchWidgetProvider smallWidget = new QuickActionSearchWidgetProviderSmall();
        QuickActionSearchWidgetProvider mediumWidget = new QuickActionSearchWidgetProviderMedium();

        SearchActivityPreferencesManager.addObserver(prefs -> {
            Context context = ContextUtils.getApplicationContext();
            if (context == null) return;
            AppWidgetManager manager = AppWidgetManager.getInstance(context);
            dinoWidget.updateWidgets(context, manager, prefs, null);
            smallWidget.updateWidgets(context, manager, prefs, null);
            mediumWidget.updateWidgets(context, manager, prefs, null);
        });
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
            @NonNull Class<? extends QuickActionSearchWidgetProvider> component,
            boolean shouldEnableWidgetComponent) {
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
}
