// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.appwidget.AppWidgetManager;
import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetType;

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
    private static class TestProvider extends QuickActionSearchWidgetProvider {
        @Override
        protected int getWidgetType() {
            return QuickActionSearchWidgetType.MEDIUM;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Context mContextMock;
    @Mock
    private AppWidgetManager mAppWidgetManagerMock;
    @Mock
    private QuickActionSearchWidgetProviderDelegate mDelegateMock;

    private QuickActionSearchWidgetProvider mWidgetProvider;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        mWidgetProvider = Mockito.spy(new TestProvider());
        mWidgetProvider.setDelegateForTesting(mDelegateMock);

        when(mContextMock.getSystemService(Context.APPWIDGET_SERVICE))
                .thenReturn(mAppWidgetManagerMock);
    }

    @Test
    @SmallTest
    public void testAppWidgetUpdateInvokesUpdateWidgets() {
        Intent appWidgetUpdateIntent = new Intent(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        appWidgetUpdateIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, WIDGET_IDS);

        mWidgetProvider.onReceive(mContextMock, appWidgetUpdateIntent);

        verify(mWidgetProvider, times(1)).onUpdate(mContextMock, mAppWidgetManagerMock, WIDGET_IDS);
        verify(mWidgetProvider, times(1)).onUpdate(any(), any(), any());

        verify(mDelegateMock, times(1))
                .updateWidgets(mContextMock, mAppWidgetManagerMock, WIDGET_IDS);
        verify(mDelegateMock, times(1)).updateWidgets(any(), any(), any());
    }
}
