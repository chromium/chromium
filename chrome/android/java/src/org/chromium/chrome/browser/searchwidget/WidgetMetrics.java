// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetProvider;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderDino;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderSearch;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class WidgetMetrics {
    // LINT.IfChange(AndroidWidgetType)
    @IntDef({
        AndroidWidgetType.SEARCH,
        AndroidWidgetType.SHORTCUTS,
        AndroidWidgetType.DINO,
        AndroidWidgetType.BOOKMARKS,
        AndroidWidgetType.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AndroidWidgetType {
        int SEARCH = 0;
        int SHORTCUTS = 1;
        int DINO = 2;
        int BOOKMARKS = 3;

        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/enums.xml:AndroidWidgetType)

    @VisibleForTesting
    static final String HISTOGRAM_INSTALLED_WIDGET_TYPES = "Android.Widget.InstalledWidgetTypes";

    @VisibleForTesting
    static final String HISTOGRAM_NUM_INSTALLED_WIDGETS = "Android.Widget.NumInstalledWidgets";

    @VisibleForTesting
    static final String HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS =
            "Android.Widget.NumInstalledWidgets.Bookmarks";

    @VisibleForTesting
    static final String HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO =
            "Android.Widget.NumInstalledWidgets.Dino";

    @VisibleForTesting
    static final String HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH =
            "Android.Widget.NumInstalledWidgets.Search";

    @VisibleForTesting
    static final String HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS =
            "Android.Widget.NumInstalledWidgets.Shortcuts";

    @VisibleForTesting
    static final String KEY_LAST_ACCOUNTING_MILESTONE =
            "org.chromium.chrome.browser.searchwidget.LAST_WIDGET_ACCOUNTING_MILESTONE";

    /**
     * Collects information about widgets installed by the user.
     *
     * <p>This method takes an action once every milestone.
     */
    public static void recordInstalledWidgetsHistograms() {
        class TrackedWidget {
            public ComponentName componentName;
            public @AndroidWidgetType int widgetType;
            public String dedicatedHistogramName;

            public TrackedWidget(
                    ComponentName componentName,
                    @AndroidWidgetType int widgetType,
                    String dedicatedHistogramName) {
                this.componentName = componentName;
                this.widgetType = widgetType;
                this.dedicatedHistogramName = dedicatedHistogramName;
            }
        }

        SharedPreferences prefsManager = ContextUtils.getAppSharedPreferences();
        long lastAccountingMilestone = prefsManager.getLong(KEY_LAST_ACCOUNTING_MILESTONE, 0);
        long currentMilestone = BuildInfo.getInstance().versionCode;

        if (lastAccountingMilestone == currentMilestone) return;

        Context context = ContextUtils.getApplicationContext();
        AppWidgetManager widgetManager = AppWidgetManager.getInstance(context);

        var trackedWidgetTypes =
                new TrackedWidget[] {
                    new TrackedWidget(
                            new ComponentName(context, SearchWidgetProvider.class.getName()),
                            AndroidWidgetType.SEARCH,
                            HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH),
                    new TrackedWidget(
                            new ComponentName(
                                    context, QuickActionSearchWidgetProviderSearch.class.getName()),
                            AndroidWidgetType.SHORTCUTS,
                            HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS),
                    new TrackedWidget(
                            new ComponentName(
                                    context, QuickActionSearchWidgetProviderDino.class.getName()),
                            AndroidWidgetType.DINO,
                            HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO),
                    new TrackedWidget(
                            new ComponentName(context, BookmarkWidgetProvider.class.getName()),
                            AndroidWidgetType.BOOKMARKS,
                            HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS)
                };

        int totalCount = 0;

        for (int index = 0; index < trackedWidgetTypes.length; index++) {
            TrackedWidget trackedWidgetType = trackedWidgetTypes[index];
            int trackedWidgetTypeCount =
                    widgetManager.getAppWidgetIds(trackedWidgetType.componentName).length;

            totalCount += trackedWidgetTypeCount;
            if (trackedWidgetTypeCount == 0) continue;

            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_INSTALLED_WIDGET_TYPES,
                    trackedWidgetType.widgetType,
                    AndroidWidgetType.COUNT);
            RecordHistogram.recordLinearCountHistogram(
                    trackedWidgetType.dedicatedHistogramName, trackedWidgetTypeCount, 1, 9, 10);
        }
        RecordHistogram.recordLinearCountHistogram(
                HISTOGRAM_NUM_INSTALLED_WIDGETS, totalCount, 1, 9, 10);

        prefsManager.edit().putLong(KEY_LAST_ACCOUNTING_MILESTONE, currentMilestone).apply();
    }
}
