// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowAppWidgetManager;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetProvider;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderDino;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderSearch;

/** Unit tests for {@link WidgetMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WidgetMetricsUnitTest {
    private static final Class<? extends AppWidgetProvider> SEARCH_COMPONENT =
            SearchWidgetProvider.class;
    private static final Class<? extends AppWidgetProvider> SHORTCUTS_COMPONENT =
            QuickActionSearchWidgetProviderSearch.class;
    private static final Class<? extends AppWidgetProvider> DINO_COMPONENT =
            QuickActionSearchWidgetProviderDino.class;
    private static final Class<? extends AppWidgetProvider> BOOKMARKS_COMPONENT =
            BookmarkWidgetProvider.class;

    private AppWidgetManager mWidgetManager =
            AppWidgetManager.getInstance(ContextUtils.getApplicationContext());
    private ShadowAppWidgetManager mShadowWidgetManager = Shadows.shadowOf(mWidgetManager);

    /** Simulates installation of |count| widgets associated with specified |provider|. */
    private void installWidget(Class<? extends AppWidgetProvider> provider, int count) {
        // Use any layout here, as we won't be manipulating remote views.
        mShadowWidgetManager.createWidgets(provider, R.layout.search_widget_template, count);
    }

    @Test
    public void recordWidgetsHistograms_noActionIfMilestoneNotChanged() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(
                        WidgetMetrics.KEY_LAST_ACCOUNTING_MILESTONE,
                        BuildInfo.getInstance().versionCode)
                .apply();

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES)
                        .build();

        WidgetMetrics.recordInstalledWidgetsHistograms();

        watcher.assertExpected();
    }

    @Test
    public void recordWidgetsHistograms_recordNoInstalledWidgets() {
        {
            HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS, 0)
                            .expectNoRecords(
                                    WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS)
                            .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO)
                            .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH)
                            .expectNoRecords(
                                    WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS)
                            .expectNoRecords(WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES)
                            .build();

            WidgetMetrics.recordInstalledWidgetsHistograms();
            watcher.assertExpected();
        }

        // Observe no subsequent records for this milestone.
        {
            HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS)
                            .build();
            WidgetMetrics.recordInstalledWidgetsHistograms();
            watcher.assertExpected();
        }
    }

    @Test
    public void recordWidgetsHistograms_searchWidgets() {
        installWidget(SEARCH_COMPONENT, 3);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS, 3)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO)
                        .expectIntRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH, 3)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.SEARCH)
                        .build();

        WidgetMetrics.recordInstalledWidgetsHistograms();
        watcher.assertExpected();
    }

    @Test
    public void recordWidgetsHistograms_shortcutsWidgets() {
        installWidget(SHORTCUTS_COMPONENT, 4);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS, 4)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS, 4)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.SHORTCUTS)
                        .build();

        WidgetMetrics.recordInstalledWidgetsHistograms();
        watcher.assertExpected();
    }

    @Test
    public void recordWidgetsHistograms_dinoWidgets() {
        installWidget(DINO_COMPONENT, 2);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS, 2)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS)
                        .expectIntRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO, 2)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.DINO)
                        .build();

        WidgetMetrics.recordInstalledWidgetsHistograms();
        watcher.assertExpected();
    }

    @Test
    public void recordWidgetsHistograms_bookmarkWidgets() {
        installWidget(BOOKMARKS_COMPONENT, 5);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS, 5)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS, 5)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH)
                        .expectNoRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.BOOKMARKS)
                        .build();

        WidgetMetrics.recordInstalledWidgetsHistograms();
        watcher.assertExpected();
    }

    @Test
    public void recordWidgetsHistograms_allWidgets() {
        installWidget(BOOKMARKS_COMPONENT, 1);
        installWidget(DINO_COMPONENT, 2);
        installWidget(SEARCH_COMPONENT, 3);
        installWidget(SHORTCUTS_COMPONENT, 4);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS, 1 + 2 + 3 + 4)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_BOOKMARKS, 1)
                        .expectIntRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_DINO, 2)
                        .expectIntRecords(WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SEARCH, 3)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_NUM_INSTALLED_WIDGETS_SHORTCUTS, 4)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.BOOKMARKS)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.DINO)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.SHORTCUTS)
                        .expectIntRecords(
                                WidgetMetrics.HISTOGRAM_INSTALLED_WIDGET_TYPES,
                                WidgetMetrics.AndroidWidgetType.SEARCH)
                        .build();

        WidgetMetrics.recordInstalledWidgetsHistograms();
        watcher.assertExpected();
    }
}
