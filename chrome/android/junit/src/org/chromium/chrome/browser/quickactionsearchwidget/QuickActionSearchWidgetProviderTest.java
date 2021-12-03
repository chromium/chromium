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
import android.content.res.Resources;
import android.os.Bundle;
import android.util.Pair;
import android.widget.RemoteViews;

import androidx.annotation.LayoutRes;
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
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider.QuickActionSearchWidgetProviderSearch;
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
        Pair<Integer, Integer> getOrientationSpecificLayoutRes(
                Context context, AppWidgetManager manager, int widgetId) {
            return new Pair<>(0, 0);
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
    private RemoteViews mRemoteViews;

    private QuickActionSearchWidgetProvider mWidgetProvider;
    private Context mContext;
    private int mXSmallWidgetMinHeightDp;
    private int mSmallWidgetMinHeightDp;
    private int mMediumWidgetMinHeightDp;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.spy(ApplicationProvider.getApplicationContext());

        // Inflate an actual RemoteViews to avoid stubbing internal methods or making
        // any other assumptions about the class.
        mRemoteViews = new RemoteViews(
                mContext.getPackageName(), R.layout.quick_action_search_widget_medium_layout);
        mWidgetProvider = Mockito.spy(new TestProvider());
        when(mContext.getSystemService(Context.APPWIDGET_SERVICE))
                .thenReturn(mAppWidgetManagerMock);
        when(mAppWidgetManagerMock.getAppWidgetOptions(anyInt())).thenReturn(mBundleMock);
        when(mDelegateMock.createWidgetRemoteViews(any(), anyInt(), any()))
                .thenReturn(mRemoteViews);

        Resources res = mContext.getResources();
        float density = res.getDisplayMetrics().density;

        // Depend on device-supplied defaults to test also on different form factors.
        // The computation below infers the size specific to a particular device running tests.
        mXSmallWidgetMinHeightDp =
                (int) (res.getDimension(R.dimen.quick_action_search_widget_xsmall_height)
                        / density);
        mSmallWidgetMinHeightDp =
                (int) (res.getDimension(R.dimen.quick_action_search_widget_small_height) / density);
        mMediumWidgetMinHeightDp =
                (int) (res.getDimension(R.dimen.quick_action_search_widget_medium_height)
                        / density);
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
        verify(mWidgetProvider, times(2)).getOrientationSpecificLayoutRes(any(), any(), anyInt());

        // We create dedicated view based on widget size for every widget instance and screen
        // orientation.
        verify(mDelegateMock, times(4)).createWidgetRemoteViews(any(), anyInt(), any());
    }

    /**
     * Test that the same delegate is used for all small size widgets.
     * This takes under consideration the fact that "dipi" and "dpi" dimensions are different
     */
    @Test
    @SmallTest
    public void testVerticalWidgetResizeOfSmallWidget() {
        doVerticalWidgetResizeOfSmallWidget(
                "Small Widget provider", new QuickActionSearchWidgetProviderSearch());
    }

    static class VerticalResizeHeightVariant {
        /** Reported widget height (in distance point) for this variant. */
        public final int heightDp;
        /** String representation (for human readable logs). */
        public final String variantName;
        /** Expected Layout Resource ID for that height. */
        public final Integer layoutRes;

        VerticalResizeHeightVariant(int heightDp, String variantName, @LayoutRes int layoutRes) {
            this.heightDp = heightDp;
            this.variantName = variantName;
            this.layoutRes = layoutRes;
        }
    }

    private void doVerticalWidgetResizeOfSmallWidget(
            String providerType, QuickActionSearchWidgetProvider provider) {
        // Validate all height pairs (exhausts the solution space).
        // This includes both reasonable and unreasonable pairs, ie. the assertion that the
        // MIN_HEIGHT <= MAX_HEIGHT does not have to hold true.
        // We want to be thorough and test both the lower and the upper boundary against what we
        // expect to be returned.
        VerticalResizeHeightVariant[] variants = new VerticalResizeHeightVariant[] {
                // The following 2 variants "should never happen" and technically violate any
                // assumptions that could be made about Android widget sizing, but we keep these to
                // verify that we're not doing anything unexpected / bad, like crashing.
                new VerticalResizeHeightVariant(0, //
                        "zero", R.layout.quick_action_search_widget_xsmall_layout),
                new VerticalResizeHeightVariant(mXSmallWidgetMinHeightDp - 1,
                        "XSmallMinHeightDp - 1", R.layout.quick_action_search_widget_xsmall_layout),

                // The following variants test every valid variant at its boundaries.
                new VerticalResizeHeightVariant(mXSmallWidgetMinHeightDp, //
                        "XSmallMinHeightDp", R.layout.quick_action_search_widget_xsmall_layout),
                new VerticalResizeHeightVariant(mXSmallWidgetMinHeightDp + 1,
                        "XSmallMinHeightDp + 1", R.layout.quick_action_search_widget_xsmall_layout),
                new VerticalResizeHeightVariant(mSmallWidgetMinHeightDp - 1, //
                        "SmallMinHeightDp - 1", R.layout.quick_action_search_widget_xsmall_layout),
                new VerticalResizeHeightVariant(mSmallWidgetMinHeightDp, //
                        "SmallMinHeightDp", R.layout.quick_action_search_widget_small_layout),
                new VerticalResizeHeightVariant(mSmallWidgetMinHeightDp + 1, //
                        "SmallMinHeightDp + 1", R.layout.quick_action_search_widget_small_layout),
                new VerticalResizeHeightVariant(mMediumWidgetMinHeightDp - 1,
                        "MediumMinHeightDp - 1", R.layout.quick_action_search_widget_small_layout),
                new VerticalResizeHeightVariant(mMediumWidgetMinHeightDp, //
                        "MediumMinHeightDp", R.layout.quick_action_search_widget_medium_layout),
                new VerticalResizeHeightVariant(mMediumWidgetMinHeightDp + 1,
                        "MediumMinHeightDp + 1", R.layout.quick_action_search_widget_medium_layout),
        };

        for (VerticalResizeHeightVariant minHeightVariant : variants) {
            for (VerticalResizeHeightVariant maxHeightVariant : variants) {
                when(mBundleMock.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT))
                        .thenReturn(minHeightVariant.heightDp);
                when(mBundleMock.getInt(AppWidgetManager.OPTION_APPWIDGET_MAX_HEIGHT))
                        .thenReturn(maxHeightVariant.heightDp);

                Pair<Integer, Integer> layouts = provider.getOrientationSpecificLayoutRes(
                        mContext, mAppWidgetManagerMock, 0);

                Assert.assertEquals(
                        "Landscape layout invalid where MIN_HEIGHT=" + minHeightVariant.variantName
                                + " and MAX_HEIGHT=" + maxHeightVariant.variantName,
                        minHeightVariant.layoutRes, layouts.first);
                Assert.assertEquals(
                        "Portrait layout invalid where MIN_HEIGHT=" + minHeightVariant.variantName
                                + " and MAX_HEIGHT=" + maxHeightVariant.variantName,
                        maxHeightVariant.layoutRes, layouts.second);
            }
        }
    }
}
