// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.appwidget.AppWidgetManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderMedium;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderSmall;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate;

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
        protected QuickActionSearchWidgetProviderDelegate getDelegate(
                Context context, AppWidgetManager manager, int widgetId) {
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

    private QuickActionSearchWidgetProvider mWidgetProvider;
    private Context mContext;
    private int mMediumWidgetMinHeight;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.spy(ApplicationProvider.getApplicationContext());

        mWidgetProvider = Mockito.spy(new TestProvider());
        when(mContext.getSystemService(Context.APPWIDGET_SERVICE))
                .thenReturn(mAppWidgetManagerMock);
        when(mAppWidgetManagerMock.getAppWidgetOptions(anyInt())).thenReturn(mBundleMock);
        // Depend on device-supplied defaults to test also on different form factors.
        // The computation below infers the size specific to a particular device running tests.
        mMediumWidgetMinHeight = (int) (mContext.getResources().getDimension(
                                                R.dimen.quick_action_search_widget_medium_height)
                / mContext.getResources().getDisplayMetrics().density);
    }

    @Test
    @SmallTest
    public void testAppWidgetUpdateInvokesUpdateWidgets() {
        Intent appWidgetUpdateIntent = new Intent(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        appWidgetUpdateIntent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, WIDGET_IDS);

        mWidgetProvider.onReceive(mContext, appWidgetUpdateIntent);

        verify(mWidgetProvider, times(1)).onUpdate(mContext, mAppWidgetManagerMock, WIDGET_IDS);
        verify(mWidgetProvider, times(1)).onUpdate(any(), any(), any());

        // We create dedicated view based on widget size for every widget instance.
        verify(mDelegateMock, times(2)).createWidgetRemoteViews(any(), any());
    }

    /**
     * Test that a different provider delegate is returned for widgets when these are scaled
     * vertically.
     * This takes under consideration the fact that "dipi" and "dpi" dimensions are different
     */
    @Test
    @SmallTest
    public void testVerticalDynamicWidgetResizeForSmallWidgetProvider() {
        doVerticalDynamicWidgetResize(
                "Small Widget provider", new QuickActionSearchWidgetProviderSmall());
        doVerticalDynamicWidgetResize(
                "Medium Widget provider", new QuickActionSearchWidgetProviderMedium());
    }

    private void doVerticalDynamicWidgetResize(
            String providerType, QuickActionSearchWidgetProvider provider) {
        when(mBundleMock.getInt(anyString()))
                .thenReturn(mMediumWidgetMinHeight - 10)
                .thenReturn(mMediumWidgetMinHeight + 10)
                .thenReturn(mMediumWidgetMinHeight - 1)
                .thenReturn(mMediumWidgetMinHeight);

        QuickActionSearchWidgetProviderDelegate delegate1 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate2 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate3 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate4 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);

        Assert.assertNotEquals(providerType, delegate1,
                delegate2); // Small and medium widget delegates are different.
        Assert.assertEquals(providerType, delegate1, delegate3); // Small widget delegates are same.
        Assert.assertEquals(
                providerType, delegate2, delegate4); // Medium widget delegates are same.
    }

    /**
     * Test that the same delegate is used for all small size widgets.
     * This takes under consideration the fact that "dipi" and "dpi" dimensions are different
     */
    @Test
    @SmallTest
    public void testVerticalWidgetResizeOfSmallWidget() {
        doVerticalWidgetResizeOfSmallWidget(
                "Small Widget provider", new QuickActionSearchWidgetProviderSmall());
        doVerticalWidgetResizeOfSmallWidget(
                "Medium Widget provider", new QuickActionSearchWidgetProviderMedium());
    }

    private void doVerticalWidgetResizeOfSmallWidget(
            String providerType, QuickActionSearchWidgetProvider provider) {
        when(mBundleMock.getInt(anyString()))
                .thenReturn(0)
                .thenReturn(1)
                .thenReturn(mMediumWidgetMinHeight - 10)
                .thenReturn(mMediumWidgetMinHeight - 1);

        QuickActionSearchWidgetProviderDelegate delegate1 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate2 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate3 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate4 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);

        Assert.assertEquals(providerType, delegate1, delegate2);
        Assert.assertEquals(providerType, delegate1, delegate3);
        Assert.assertEquals(providerType, delegate1, delegate4);
    }

    /**
     * Test that the same delegate is used for all medium size widgets.
     * This takes under consideration the fact that "dipi" and "dpi" dimensions are different
     */
    @Test
    @SmallTest
    public void testVerticalWidgetResizeOfMediumWidget() {
        doVerticalWidgetResizeOfMediumWidget(
                "Small Widget provider", new QuickActionSearchWidgetProviderSmall());
        doVerticalWidgetResizeOfMediumWidget(
                "Medium Widget provider", new QuickActionSearchWidgetProviderMedium());
    }

    private void doVerticalWidgetResizeOfMediumWidget(
            String providerType, QuickActionSearchWidgetProvider provider) {
        when(mBundleMock.getInt(anyString()))
                .thenReturn(mMediumWidgetMinHeight)
                .thenReturn(mMediumWidgetMinHeight + 1)
                .thenReturn(mMediumWidgetMinHeight * 10);

        QuickActionSearchWidgetProviderDelegate delegate1 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate2 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);
        QuickActionSearchWidgetProviderDelegate delegate3 =
                provider.getDelegate(mContext, mAppWidgetManagerMock, 1);

        Assert.assertEquals(providerType, delegate1, delegate2);
        Assert.assertEquals(providerType, delegate1, delegate3);
    }
}
