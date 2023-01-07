// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.widget.RemoteViews;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
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
    public static class QuickActionSearchWidgetProviderSearch
            extends QuickActionSearchWidgetProvider {
        @Override
        @NonNull
        RemoteViews getRemoteViews(@NonNull Context context,
                @NonNull SearchActivityPreferences prefs, @NonNull AppWidgetManager manager,
                int widgetId) {
            Bundle options = manager.getAppWidgetOptions(widgetId);
            return getDelegate().createSearchWidgetRemoteViews(context, prefs,
                    getPortraitModeTargetAreaWidth(options),
                    getPortraitModeTargetAreaHeight(options),
                    getLandscapeModeTargetAreaWidth(options),
                    getLandscapeModeTargetAreaHeight(options));
        }
    }

    /** Returns the widget area width in portrait orientation (dp). */
    private static int getPortraitModeTargetAreaWidth(Bundle options) {
        return options.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH);
    }

    /** Returns the widget area height in portrait orientation (dp). */
    private static int getPortraitModeTargetAreaHeight(Bundle options) {
        return options.getInt(AppWidgetManager.OPTION_APPWIDGET_MAX_HEIGHT);
    }

    /** Returns the widget area width in landscape orientation (dp). */
    private static int getLandscapeModeTargetAreaWidth(Bundle options) {
        return options.getInt(AppWidgetManager.OPTION_APPWIDGET_MAX_WIDTH);
    }

    /** Returns the widget area height in landscape orientation (dp). */
    private static int getLandscapeModeTargetAreaHeight(Bundle options) {
        return options.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT);
    }

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that
     * only contains a touch surface for launching the Dino game.
     */
    public static class QuickActionSearchWidgetProviderDino
            extends QuickActionSearchWidgetProvider {
        @Override
        @NonNull
        RemoteViews getRemoteViews(@NonNull Context context,
                @NonNull SearchActivityPreferences prefs, @NonNull AppWidgetManager manager,
                int widgetId) {
            Bundle options = manager.getAppWidgetOptions(widgetId);
            return getDelegate().createDinoWidgetRemoteViews(context, prefs,
                    getPortraitModeTargetAreaWidth(options),
                    getPortraitModeTargetAreaHeight(options),
                    getLandscapeModeTargetAreaWidth(options),
                    getLandscapeModeTargetAreaHeight(options));
        }
    }

    private static @Nullable QuickActionSearchWidgetProviderDelegate sDelegate;

    @Override
    public void onUpdate(@NonNull Context context, @NonNull AppWidgetManager manager,
            @Nullable int[] widgetIds) {
        updateWidgets(context, manager, SearchActivityPreferencesManager.getCurrent(), widgetIds);
    }

    @Override
    public void onAppWidgetOptionsChanged(
            Context context, AppWidgetManager manager, int widgetId, Bundle newOptions) {
        super.onAppWidgetOptionsChanged(context, manager, widgetId, newOptions);
        onUpdate(context, manager, new int[] {widgetId});
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
            manager.updateAppWidget(
                    widgetId, getRemoteViews(context, preferences, manager, widgetId));
        }
    }

    /**
     * Get (create if necessary) an instance of QuickActionSearchWidgetProviderDelegate.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @NonNull
    protected QuickActionSearchWidgetProviderDelegate getDelegate() {
        if (sDelegate != null) return sDelegate;

        Context context = ContextUtils.getApplicationContext();
        ComponentName searchActivityComponent = new ComponentName(context, SearchActivity.class);
        Intent trustedIncognitoIntent =
                IntentHandler.createTrustedOpenNewTabIntent(context, /*incognito=*/true);
        trustedIncognitoIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        trustedIncognitoIntent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        Intent dinoIntent = createDinoIntent(context);

        sDelegate = new QuickActionSearchWidgetProviderDelegate(
                context, searchActivityComponent, trustedIncognitoIntent, dinoIntent);
        return sDelegate;
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
     * Acquire screen orientation specific layouts that will be applied to the
     * widget.
     * The two layouts represent screen orientations in Landscape and Portrait mode.
     *
     * @param context Current context.
     * @param manager The AppWidgetManager instance to query widget info.
     * @param widgetId The widget to get the delegate for.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    abstract @NonNull RemoteViews getRemoteViews(@NonNull Context context,
            @NonNull SearchActivityPreferences prefs, @NonNull AppWidgetManager manager,
            int widgetId);

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
        QuickActionSearchWidgetProvider dinoWidget = new QuickActionSearchWidgetProviderDino();
        QuickActionSearchWidgetProvider smallWidget = new QuickActionSearchWidgetProviderSearch();

        PostTask.postTask(TaskTraits.BEST_EFFORT, () -> {
            // Make the Widget available to all Chrome users who participated in an experiment in
            // the past. This can trigger disk access. Unfortunately, we need to keep it for a
            // little bit longer -- see: https://crbug.com/1309116
            setWidgetEnabled(true, true);
        });

        SearchActivityPreferencesManager.addObserver(prefs -> {
            Context context = ContextUtils.getApplicationContext();
            if (context == null) return;
            AppWidgetManager manager = AppWidgetManager.getInstance(context);
            if (manager == null) {
                // The device does not support widgets. Abort.
                return;
            }
            dinoWidget.updateWidgets(context, manager, prefs, null);
            smallWidget.updateWidgets(context, manager, prefs, null);
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
                QuickActionSearchWidgetProviderSearch.class, shouldEnableQuickActionSearchWidget);
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
