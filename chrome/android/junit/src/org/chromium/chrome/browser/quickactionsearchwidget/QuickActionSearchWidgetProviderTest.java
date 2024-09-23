// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.appwidget.AppWidgetManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.SizeF;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderDino;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests for the (@link QuickActionSearchWidgetProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLog.class})
public class QuickActionSearchWidgetProviderTest {
    private static final int WIDGET_A_ID = 123;
    private static final int WIDGET_B_ID = 987;
    // These are random unique identifiers, the value of these numbers have no special meaning.
    // The number of identifiers has no particular meaning either.
    private static final int[] WIDGET_IDS = {WIDGET_A_ID, WIDGET_B_ID};
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock AppWidgetManager mAppWidgetManagerMock;

    private SearchActivityPreferences mPreferences;
    private QuickActionSearchWidgetProvider mWidgetProvider;
    private Context mContext;
    private Intent mIntent;
    private Bundle mOptionsWidgetA;
    private Bundle mOptionsWidgetB;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mContext = Mockito.spy(ApplicationProvider.getApplicationContext());
        mOptionsWidgetA = new Bundle();
        mOptionsWidgetB = new Bundle();
        mPreferences =
                new SearchActivityPreferences(
                        "Search Engine", new GURL("https://search.engine.com"), true, true, true);

        // Inflate an actual RemoteViews to avoid stubbing internal methods or making
        // any other assumptions about the class.
        mWidgetProvider = Mockito.spy(new QuickActionSearchWidgetProviderDino());
        when(mContext.getSystemService(Context.APPWIDGET_SERVICE))
                .thenReturn(mAppWidgetManagerMock);
        when(mAppWidgetManagerMock.getAppWidgetOptions(eq(WIDGET_A_ID)))
                .thenReturn(mOptionsWidgetA);
        when(mAppWidgetManagerMock.getAppWidgetOptions(eq(WIDGET_B_ID)))
                .thenReturn(mOptionsWidgetB);

        // Blanket intent that defines which widget IDs we would be testing here.
        mIntent = new Intent();
        mIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, WIDGET_IDS);
    }

    private void updateReportedWidgetSizes(Bundle options, SizeF portrait, SizeF landscape) {
        // - Portrait mode is narrow and tall, hence MIN_WIDTH and MAX_HEIGHT.
        // - Landscape mode is wide and short, hence MAX_WIDTH and MIN_HEIGHT.
        options.putInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, (int) portrait.getWidth());
        options.putInt(AppWidgetManager.OPTION_APPWIDGET_MAX_HEIGHT, (int) portrait.getHeight());
        options.putInt(AppWidgetManager.OPTION_APPWIDGET_MAX_WIDTH, (int) landscape.getWidth());
        options.putInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT, (int) landscape.getHeight());
    }

    @Test
    @SmallTest
    public void testAppWidgetInstallationCreatesWidgets() {
        mIntent.setAction(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        updateReportedWidgetSizes(mOptionsWidgetA, new SizeF(80, 80), new SizeF(400, 40));
        updateReportedWidgetSizes(mOptionsWidgetB, new SizeF(30, 10), new SizeF(100, 30));
        mWidgetProvider.onReceive(mContext, mIntent);

        // There are 2 fake widgets to be created, and each should be created for portrait and
        // landscape screen orientation.
        // Widget A, Portrait.
        verify(mWidgetProvider).createWidget(any(), any(), eq(80), eq(80));
        // Widget A, Landscape.
        verify(mWidgetProvider).createWidget(any(), any(), eq(400), eq(40));
        // Widget B, Portrait.
        verify(mWidgetProvider).createWidget(any(), any(), eq(30), eq(10));
        // Widget B, Landscape.
        verify(mWidgetProvider).createWidget(any(), any(), eq(100), eq(30));
        // 4 total, no more.
        verify(mWidgetProvider, times(4)).createWidget(any(), any(), anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testAppWidgetResizeUpdatesWidgets() {
        updateReportedWidgetSizes(mOptionsWidgetA, new SizeF(80, 80), new SizeF(400, 40));
        updateReportedWidgetSizes(mOptionsWidgetB, new SizeF(30, 10), new SizeF(100, 30));
        mIntent.setAction(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        mWidgetProvider.onReceive(mContext, mIntent);

        verify(mWidgetProvider).createWidget(any(), any(), eq(80), eq(80));
        verify(mWidgetProvider).createWidget(any(), any(), eq(400), eq(40));
        verify(mWidgetProvider).createWidget(any(), any(), eq(30), eq(10));
        verify(mWidgetProvider).createWidget(any(), any(), eq(100), eq(30));
        verify(mWidgetProvider, times(4)).createWidget(any(), any(), anyInt(), anyInt());

        clearInvocations(mWidgetProvider);

        // OPTIONS_CHANGED specifies which particular widget needs to be updated. Make sure we
        // reflect that here. Below, we flip the sizes so that Widget B uses sizes of Widget A and
        // vice versa.
        when(mAppWidgetManagerMock.getAppWidgetOptions(eq(WIDGET_A_ID)))
                .thenReturn(mOptionsWidgetB);
        when(mAppWidgetManagerMock.getAppWidgetOptions(eq(WIDGET_B_ID)))
                .thenReturn(mOptionsWidgetA);

        // First, resize Widget A with old Widget B size specs.
        mIntent.setAction(AppWidgetManager.ACTION_APPWIDGET_OPTIONS_CHANGED);
        mIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, WIDGET_A_ID);
        mIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_OPTIONS, mOptionsWidgetB);
        mWidgetProvider.onReceive(mContext, mIntent);

        verify(mWidgetProvider).createWidget(any(), any(), eq(30), eq(10));
        verify(mWidgetProvider).createWidget(any(), any(), eq(100), eq(30));
        verify(mWidgetProvider, times(2)).createWidget(any(), any(), anyInt(), anyInt());
        clearInvocations(mWidgetProvider);

        // Next, resize Widget B with old Widget A size specs.
        mIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, WIDGET_B_ID);
        mIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_OPTIONS, mOptionsWidgetA);
        mWidgetProvider.onReceive(mContext, mIntent);
        verify(mWidgetProvider).createWidget(any(), any(), eq(80), eq(80));
        verify(mWidgetProvider).createWidget(any(), any(), eq(400), eq(40));
        verify(mWidgetProvider, times(2)).createWidget(any(), any(), anyInt(), anyInt());
    }

    @Test
    @SmallTest
    @Config(sdk = Build.VERSION_CODES.S)
    public void testCreateWidgetsFromFallbackValues_missingSizes() {
        updateReportedWidgetSizes(mOptionsWidgetA, new SizeF(80, 80), new SizeF(400, 40));
        mWidgetProvider.getRemoteViews(mContext, mPreferences, mOptionsWidgetA);

        // There are 2 fake widgets that we work with, so expect both being evaluated
        // One for portrait mode (max_width, min_height) and
        // One for landscape mode (min_width, max_height).
        verify(mWidgetProvider).createWidget(any(), any(), eq(400), eq(40));
        verify(mWidgetProvider).createWidget(any(), any(), eq(80), eq(80));
        verify(mWidgetProvider, times(2)).createWidget(any(), any(), anyInt(), anyInt());
    }

    @Test
    @SmallTest
    @Config(sdk = Build.VERSION_CODES.S)
    public void testCreateWidgetFromFallbackValues_emptySizes() {
        updateReportedWidgetSizes(mOptionsWidgetA, new SizeF(80, 80), new SizeF(400, 40));
        // Lastly, set the empty array of sizes.
        mOptionsWidgetA.putParcelableArrayList(
                AppWidgetManager.OPTION_APPWIDGET_SIZES, new ArrayList<SizeF>());
        mWidgetProvider.getRemoteViews(mContext, mPreferences, mOptionsWidgetA);

        // There are 2 fake widgets that we work with, so expect both being evaluated
        // One for portrait mode (max_width, min_height) and
        // One for landscape mode (min_width, max_height).
        verify(mWidgetProvider).createWidget(any(), any(), eq(400), eq(40));
        verify(mWidgetProvider).createWidget(any(), any(), eq(80), eq(80));
        verify(mWidgetProvider, times(2)).createWidget(any(), any(), anyInt(), anyInt());
    }

    @Test
    @SmallTest
    @Config(sdk = Build.VERSION_CODES.S)
    public void testCreateWidgetFromSizeSpecs() {
        updateReportedWidgetSizes(mOptionsWidgetA, new SizeF(80, 80), new SizeF(400, 40));
        // Lastly, set a different array of sizes and confirm it is used instead.
        mOptionsWidgetA.putParcelableArrayList(
                AppWidgetManager.OPTION_APPWIDGET_SIZES,
                new ArrayList<SizeF>(Arrays.asList(new SizeF(50, 50))));
        mWidgetProvider.getRemoteViews(mContext, mPreferences, mOptionsWidgetA);

        // Only one call is expected here, because we declare only one size in our list.
        verify(mWidgetProvider).createWidget(any(), any(), eq(50), eq(50));
        verify(mWidgetProvider, times(1)).createWidget(any(), any(), anyInt(), anyInt());
    }

    @Test
    @SmallTest
    @Config(sdk = Build.VERSION_CODES.R)
    public void testCreateWidgetFromLegacyMeasurements() {
        updateReportedWidgetSizes(mOptionsWidgetA, new SizeF(80, 80), new SizeF(400, 40));
        // Define new sizes. These must be ignored, because RemoteViews constructor is not
        // supported.
        mOptionsWidgetA.putParcelableArrayList(
                AppWidgetManager.OPTION_APPWIDGET_SIZES,
                new ArrayList<SizeF>(Arrays.asList(new SizeF(50, 50))));
        mWidgetProvider.getRemoteViews(mContext, mPreferences, mOptionsWidgetA);

        // There are 2 fake widgets that we work with, so expect both being evaluated
        // One for portrait mode (max_width, min_height) and
        // One for landscape mode (min_width, max_height).
        verify(mWidgetProvider).createWidget(any(), any(), eq(400), eq(40));
        verify(mWidgetProvider).createWidget(any(), any(), eq(80), eq(80));
        verify(mWidgetProvider, times(2)).createWidget(any(), any(), anyInt(), anyInt());
    }
}
