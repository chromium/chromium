// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.appwidget.AppWidgetManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.widget.RemoteViews;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;

/**
 * Tests for the (@link QuickActionSearchWidgetProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class QuickActionSearchWidgetProviderTest {
    // These are random unique identifiers, the value of these numbers have no special meaning.
    // The number of identifiers has no particular meaning either.
    private static final int[] WIDGET_IDS = {1, 2};

    /**
     * A sub class of {@link QuickActionSearchWidgetProvider} for testing, since
     * QuickActionSearchWidgetProvider is abstract.
     */
    private class TestProvider extends QuickActionSearchWidgetProvider {
        @Override
        RemoteViews getRemoteViews(Context context, SearchActivityPreferences prefs,
                AppWidgetManager manager, int widgetId) {
            return mRemoteViews;
        }

        @Override
        protected QuickActionSearchWidgetProviderDelegate getDelegate() {
            return mDelegateMock;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private AppWidgetManager mAppWidgetManagerMock;
    @Mock
    private QuickActionSearchWidgetProviderDelegate mDelegateMock;
    @Mock
    private Bundle mBundleMock;
    @Mock
    private RemoteViews mRemoteViews;

    private QuickActionSearchWidgetProvider mWidgetProvider;
    private Context mContext;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.spy(ApplicationProvider.getApplicationContext());

        // Inflate an actual RemoteViews to avoid stubbing internal methods or making
        // any other assumptions about the class.
        mWidgetProvider = Mockito.spy(new TestProvider());
        when(mContext.getSystemService(Context.APPWIDGET_SERVICE))
                .thenReturn(mAppWidgetManagerMock);
        when(mAppWidgetManagerMock.getAppWidgetOptions(anyInt())).thenReturn(mBundleMock);
    }

    @Test
    @SmallTest
    public void testAppWidgetUpdateInvokesUpdateWidgets() {
        Intent appWidgetUpdateIntent = new Intent(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        appWidgetUpdateIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, WIDGET_IDS);

        mWidgetProvider.onReceive(mContext, appWidgetUpdateIntent);

        verify(mWidgetProvider, times(1)).onUpdate(mContext, mAppWidgetManagerMock, WIDGET_IDS);
        verify(mWidgetProvider, times(1)).onUpdate(any(), any(), any());

        // There are 2 fake widgets that we work with, so expect both being evaluated
        verify(mWidgetProvider, times(2)).getRemoteViews(any(), any(), any(), anyInt());
    }
}
