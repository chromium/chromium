// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.appwidget.AppWidgetManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.RemoteViews;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for the BookmarkWidgetProvider. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class BookmarkWidgetProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AppWidgetManager mAppWidgetManager;

    private Context mContext;
    private BookmarkWidgetProvider mProvider;

    private static final int WIDGET_ID = 1;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mProvider = new BookmarkWidgetProvider();
    }

    @Test
    @SmallTest
    public void testOnUpdate() {
        int[] widgetIds = new int[] {WIDGET_ID};
        Bundle options = new Bundle();
        options.putInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, 200);
        when(mAppWidgetManager.getAppWidgetOptions(anyInt())).thenReturn(options);

        mProvider.onUpdate(mContext, mAppWidgetManager, widgetIds);

        ArgumentCaptor<RemoteViews> remoteViewsCaptor = ArgumentCaptor.forClass(RemoteViews.class);

        verify(mAppWidgetManager).notifyAppWidgetViewDataChanged(WIDGET_ID, R.id.bookmarks_list);
        verify(mAppWidgetManager).updateAppWidget(eq(WIDGET_ID), remoteViewsCaptor.capture());
        RemoteViews capturedViews = remoteViewsCaptor.getValue();
        assertEquals(
                "Widget should use the standard layout.",
                R.layout.bookmark_widget,
                capturedViews.getLayoutId());
    }

    @Test
    @SmallTest
    public void testOnAppWidgetOptionsChanged_updatesToIconsOnlyLayout() {
        Bundle newOptions = new Bundle();
        // Set a width below the threshold to trigger the icons-only layout.
        newOptions.putInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, 109);
        when(mAppWidgetManager.getAppWidgetOptions(WIDGET_ID)).thenReturn(newOptions);

        mProvider.onAppWidgetOptionsChanged(mContext, mAppWidgetManager, WIDGET_ID, newOptions);

        ArgumentCaptor<RemoteViews> captor = ArgumentCaptor.forClass(RemoteViews.class);
        verify(mAppWidgetManager).updateAppWidget(eq(WIDGET_ID), captor.capture());

        RemoteViews views = captor.getValue();
        assertEquals(
                "Widget should have switched to icons-only layout after resize.",
                R.layout.bookmark_widget_icons_only,
                views.getLayoutId());
    }

    @Test
    @SmallTest
    public void testOnAppWidgetOptionsChanged_updatesToNormalLayout() {
        Bundle newOptions = new Bundle();
        // Set a width above the threshold to ensure the standard layout is used.
        newOptions.putInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, 300);
        when(mAppWidgetManager.getAppWidgetOptions(WIDGET_ID)).thenReturn(newOptions);

        mProvider.onAppWidgetOptionsChanged(mContext, mAppWidgetManager, WIDGET_ID, newOptions);

        ArgumentCaptor<RemoteViews> captor = ArgumentCaptor.forClass(RemoteViews.class);
        verify(mAppWidgetManager).updateAppWidget(eq(WIDGET_ID), captor.capture());

        RemoteViews views = captor.getValue();
        assertEquals(
                "Widget should be using the standard layout after resize.",
                R.layout.bookmark_widget,
                views.getLayoutId());
    }

    @Test
    @SmallTest
    public void testOnDeleted_cleansUpWidgetState() {
        int[] widgetIdsToDelete = new int[] {WIDGET_ID};

        // Add some data to the widget's shared preferences to ensure it's not empty.
        SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
        prefs.edit().putString("test_key", "test_value").commit();

        mProvider.onDeleted(mContext, widgetIdsToDelete);

        assertTrue(
                "SharedPreferences for the deleted widget should be empty.",
                prefs.getAll().isEmpty());
    }
}
