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
import android.os.Build;
import android.os.Bundle;
import android.util.ArrayMap;
import android.util.SizeF;
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
import org.chromium.chrome.browser.searchwidget.SearchActivityClientImpl;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.ArrayList;
import java.util.Map;

/**
 * {@link AppWidgetProvider} for a widget that provides an entry point for users to quickly perform
 * actions in Chrome.
 */
public abstract class QuickActionSearchWidgetProvider extends AppWidgetProvider {
    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that can
     * resize.
     */
    public static class QuickActionSearchWidgetProviderSearch
            extends QuickActionSearchWidgetProvider {
        @Override
        @NonNull
        RemoteViews createWidget(
                @NonNull Context context,
                @NonNull SearchActivityPreferences prefs,
                int areaWidthDp,
                int areaHeightDp) {
            return getDelegate()
                    .createSearchWidgetRemoteViews(
                            context,
                            new SearchActivityClientImpl(),
                            prefs,
                            areaWidthDp,
                            areaHeightDp);
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
     * A sub class of {@link QuickActionSearchWidgetProvider} that provides the widget that only
     * contains a touch surface for launching the Dino game.
     */
    public static class QuickActionSearchWidgetProviderDino
            extends QuickActionSearchWidgetProvider {
        @Override
        @NonNull
        RemoteViews createWidget(
                @NonNull Context context,
                @NonNull SearchActivityPreferences prefs,
                int areaWidthDp,
                int areaHeightDp) {
            return getDelegate()
                    .createDinoWidgetRemoteViews(
                            context,
                            new SearchActivityClientImpl(),
                            prefs,
                            areaWidthDp,
                            areaHeightDp);
        }
    }

    private static @Nullable QuickActionSearchWidgetProviderDelegate sDelegate;

    @Override
    public void onUpdate(
            @NonNull Context context,
            @NonNull AppWidgetManager manager,
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
    void updateWidgets(
            @NonNull Context context,
            @NonNull AppWidgetManager manager,
            @NonNull SearchActivityPreferences preferences,
            @NonNull int[] widgetIds) {
        if (widgetIds == null) {
            // Query all widgets associated with this component.
            widgetIds = manager.getAppWidgetIds(new ComponentName(context, getClass().getName()));
        }

        for (int index = 0; index < widgetIds.length; index++) {
            int widgetId = widgetIds[index];
            Bundle options = manager.getAppWidgetOptions(widgetId);
            manager.updateAppWidget(widgetId, getRemoteViews(context, preferences, options));
        }
    }

    /** Get (create if necessary) an instance of QuickActionSearchWidgetProviderDelegate. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    protected @NonNull QuickActionSearchWidgetProviderDelegate getDelegate() {
        if (sDelegate != null) return sDelegate;

        Context context = ContextUtils.getApplicationContext();
        Intent trustedIncognitoIntent =
                IntentHandler.createTrustedOpenNewTabIntent(context, /* incognito= */ true);
        trustedIncognitoIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        trustedIncognitoIntent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        Intent dinoIntent = createDinoIntent(context);

        sDelegate =
                new QuickActionSearchWidgetProviderDelegate(
                        context, trustedIncognitoIntent, dinoIntent);
        return sDelegate;
    }

    /**
     * Creates an intent to launch a new tab with chrome://dino/ URL.
     *
     * @param context The context from which the intent is being created.
     * @return An intent to launch a tab with a new tab with chrome://dino/ URL.
     */
    private static Intent createDinoIntent(final Context context) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(UrlConstants.CHROME_DINO_URL));
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }

    /**
     * Construct the widget for specific dimensions.
     *
     * @param context Current context.
     * @param prefs Widget settings and feature availability.
     * @param areaWidthDp The width of the widget area, expressed in Dp.
     * @param areaHeightDp The height of the widget area, expressed in Dp.
     * @return RemoteViews description for a single widget layout.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    abstract @NonNull RemoteViews createWidget(
            @NonNull Context context,
            @NonNull SearchActivityPreferences prefs,
            int areaWidthDp,
            int areaHeightDp);

    /**
     * Acquire the RemoteViews that represent the widget.
     *
     * @param context Current context.
     * @param prefs Widget settings and feature availability.
     * @param options Options bundle passed by AppWidgetManager.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @NonNull
    RemoteViews getRemoteViews(
            @NonNull Context context,
            @NonNull SearchActivityPreferences prefs,
            @NonNull Bundle options) {
        var views = getSizeMappedRemoteViews(context, prefs, options);
        if (views != null) {
            return views;
        }
        return getOrientationSpecificRemoteViews(context, prefs, options);
    }

    /**
     * Acquire screen orientation specific layouts that will be applied to the widget.
     *
     * @param context Current context.
     * @param prefs Widget settings and feature availability.
     * @param options Widget parameters passed by the AppWidgetManager.
     * @return RemoteViews describing widget for landscape and portrait screen orientations.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @NonNull
    RemoteViews getOrientationSpecificRemoteViews(
            @NonNull Context context,
            @NonNull SearchActivityPreferences prefs,
            @NonNull Bundle options) {
        var portraitViews =
                createWidget(
                        context,
                        prefs,
                        getPortraitModeTargetAreaWidth(options),
                        getPortraitModeTargetAreaHeight(options));

        var landscapeViews =
                createWidget(
                        context,
                        prefs,
                        getLandscapeModeTargetAreaWidth(options),
                        getLandscapeModeTargetAreaHeight(options));

        return new RemoteViews(landscapeViews, portraitViews);
    }

    /**
     * Acquire size-specific layouts that will be applied to the widget.
     *
     * @param context Current context.
     * @param prefs Widget settings and feature availability.
     * @param options Widget parameters passed by the AppWidgetManager.
     * @return RemoteViews describing widget for all sizes requested by the AppWidgetManager, or
     *     null, if the AppWidgetManager did not specify the sizes.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @Nullable
    RemoteViews getSizeMappedRemoteViews(
            @NonNull Context context,
            @NonNull SearchActivityPreferences prefs,
            @NonNull Bundle options) {
        // On Android S and above, attempt to build widget from supplied array of sizes.
        // This is reserved to Android S because appropriate RemoteViews constructor may not be
        // available.
        // Note that the creation may still fail, if the launcher is unable to offer appropriate
        // details.
        // Check for supported system version.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return null;
        }

        ArrayList<SizeF> sizes =
                options.getParcelableArrayList(AppWidgetManager.OPTION_APPWIDGET_SIZES);
        if (sizes == null || sizes.isEmpty()) {
            return null;
        }
        Map<SizeF, RemoteViews> mappings = new ArrayMap<>();

        for (var size : sizes) {
            mappings.put(
                    size,
                    createWidget(context, prefs, (int) size.getWidth(), (int) size.getHeight()));
        }
        return new RemoteViews(mappings);
    }

    /**
     * This function initializes the QuickActionSearchWidgetProvider component. Namely, this
     * function enables the component for users who have the QUICK_ACTION_SEARCH_WIDGET flag
     * enabled.
     *
     * <p>Note that due to b/189087746, the widget cannot be disabled be default, as a result, we
     * must enable/disable the widget programmatically here.
     *
     * <p>This function is expected to be called exactly once after native libraries are
     * initialized.
     */
    public static void initialize() {
        QuickActionSearchWidgetProvider dinoWidget = new QuickActionSearchWidgetProviderDino();
        QuickActionSearchWidgetProvider smallWidget = new QuickActionSearchWidgetProviderSearch();

        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    // Make the Widget available to all Chrome users who participated in an
                    // experiment in the past. This can trigger disk access. Unfortunately,
                    // we need to keep it for a little bit longer -- see:
                    // https://crbug.com/1309116
                    setWidgetEnabled(true, true);
                });

        SearchActivityPreferencesManager.addObserver(
                prefs -> {
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
     *     should be enabled or not.
     * @param shouldEnableDinoVariant a boolean indicating whether the widget component of the Dino
     *     variant should be enabled.
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
     *     widget that is to be disabled.
     * @param shouldEnableWidgetComponent a boolean indicating whether the widget component should
     *     be enabled or not.
     */
    private static void setWidgetComponentEnabled(
            @NonNull Class<? extends QuickActionSearchWidgetProvider> component,
            boolean shouldEnableWidgetComponent) {
        // The initialization must be performed on a background thread because the following logic
        // can trigger disk access. The PostTask in ProcessInitializationHandler can be removed once
        // the experimentation phase is over.
        ThreadUtils.assertOnBackgroundThread();
        Context context = ContextUtils.getApplicationContext();

        int componentEnabledState =
                shouldEnableWidgetComponent
                        ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                        : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        ComponentName componentName = new ComponentName(context, component);
        context.getPackageManager()
                .setComponentEnabledSetting(
                        componentName, componentEnabledState, PackageManager.DONT_KILL_APP);
    }
}
