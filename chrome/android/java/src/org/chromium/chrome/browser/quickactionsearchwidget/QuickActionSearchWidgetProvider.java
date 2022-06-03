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
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.util.DisplayMetrics;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * {@link AppWidgetProvider} for a widget that provides an entry point for users to quickly perform
 * actions in Chrome.
 */
public abstract class QuickActionSearchWidgetProvider extends AppWidgetProvider {
    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * can resize.
     */
    public static class QuickActionSearchWidgetResizableProvider
            extends QuickActionSearchWidgetProvider {
        protected static @Nullable QuickActionSearchWidgetProviderDelegate sSmallWidgetDelegate;
        protected static @Nullable QuickActionSearchWidgetProviderDelegate sMediumWidgetDelegate;

        @Override
        public void onAppWidgetOptionsChanged(
                Context context, AppWidgetManager manager, int widgetId, Bundle newOptions) {
            onUpdate(context, manager, new int[] {widgetId});
            super.onAppWidgetOptionsChanged(context, manager, widgetId, newOptions);
        }

        @Override
        @NonNull
        QuickActionSearchWidgetProviderDelegate getDelegate(
                Context context, AppWidgetManager manager, int widgetId) {
            Bundle options = manager.getAppWidgetOptions(widgetId);
            DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
            boolean isLandscapeMode = context.getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_LANDSCAPE;

            // MIN_HEIGHT is reported for landscape mode, whereas MAX_HEIGHT is used with portrait.
            int newWidgetHeightDp =
                    options.getInt(isLandscapeMode ? AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT
                                                   : AppWidgetManager.OPTION_APPWIDGET_MAX_HEIGHT);
            float mediumWidgetMinHeightDp =
                    context.getResources().getDimension(
                            R.dimen.quick_action_search_widget_medium_height)
                    / displayMetrics.density;

            if (newWidgetHeightDp >= mediumWidgetMinHeightDp) {
                return getMediumWidgetDelegate();
            }

            return getSmallWidgetDelegate();
        }

        private @NonNull QuickActionSearchWidgetProviderDelegate getSmallWidgetDelegate() {
            if (sSmallWidgetDelegate == null) {
                sSmallWidgetDelegate =
                        createDelegate(R.layout.quick_action_search_widget_small_layout);
            }
            return sSmallWidgetDelegate;
        }

        private @NonNull QuickActionSearchWidgetProviderDelegate getMediumWidgetDelegate() {
            if (sMediumWidgetDelegate == null) {
                sMediumWidgetDelegate =
                        createDelegate(R.layout.quick_action_search_widget_medium_layout);
            }
            return sMediumWidgetDelegate;
        }
    }
    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * initially has the small layout.
     * Layout constraints are defined in the widget info xml file.
     * Dedicated provider required by manifest declaration.
     */
    public static class QuickActionSearchWidgetProviderSmall
            extends QuickActionSearchWidgetResizableProvider {}

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * initially has the medium layout.
     * Layout constraints are defined in the widget info xml file.
     * Dedicated provider required by manifest declaration.
     */
    public static class QuickActionSearchWidgetProviderMedium
            extends QuickActionSearchWidgetResizableProvider {}

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * only contains a touch surface for launching the Dino game.
     */
    public static class QuickActionSearchWidgetProviderDino
            extends QuickActionSearchWidgetProvider {
        private static QuickActionSearchWidgetProviderDelegate sDelegate;

        @Override
        protected QuickActionSearchWidgetProviderDelegate getDelegate(
                Context context, AppWidgetManager manager, int widgetId) {
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

        for (int index = 0; index < widgetIds.length; index++) {
            int widgetId = widgetIds[index];

            manager.updateAppWidget(widgetId,
                    getDelegate(context, manager, widgetId)
                            .createWidgetRemoteViews(context, preferences));
        }
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
        ComponentName searchActivityComponent = new ComponentName(context, SearchActivity.class);
        Intent trustedIncognitoIntent =
                IntentHandler.createTrustedOpenNewTabIntent(context, /*incognito=*/true);
        trustedIncognitoIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        trustedIncognitoIntent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        Intent dinoIntent = createDinoIntent(context);

        return new QuickActionSearchWidgetProviderDelegate(
                layout, searchActivityComponent, trustedIncognitoIntent, dinoIntent);
    }

    /**
     * Creates an intent to launch a new tab with chrome://dino/ URL.
     *
     * @param context The context from which the intent is being created.
     * @return An intent to launch a tab with a new tab with chrome://dino/ URL.
     */
    private static Intent createDinoIntent(final Context context) {
        // We concatenate the forward slash to the URL since if a Dino tab already exists, we would
        // like to reuse it. In order to determine if there is an existing Dino tab,
        // ChromeTabbedActivity will check by comparing URLs of existing tabs to the URL of our
        // intent. If there is an existing Dino tab, it would have a forward slash appended to the
        // end of its URL, so our URL must have a forward slash to match.
        String chromeDinoUrl = UrlConstants.CHROME_DINO_URL + "/";

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(chromeDinoUrl));
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }

    /**
     * This function lazily initializes and returns the
     * {@link QuickActionSearchWidgetProviderDelegate}
     * for this instance.
     * <p>
     * We don't initialize the delegate in the constructor because creation of the
     * QuickActionSearchWidgetProvider is done by the system.
     *
     * @param context Current context.
     * @param manager The AppWidgetManager instance to query widget info.
     * @param widgetId The widget to get the delegate for.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    abstract QuickActionSearchWidgetProviderDelegate getDelegate(
            Context context, AppWidgetManager manager, int widgetId);

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
