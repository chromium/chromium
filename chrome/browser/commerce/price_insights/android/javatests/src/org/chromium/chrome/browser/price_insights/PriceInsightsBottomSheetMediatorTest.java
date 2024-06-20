// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link PriceInsightsBottomSheetMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
public class PriceInsightsBottomSheetMediatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mMockContext;
    @Mock private ShoppingService mMockShoppingService;
    @Mock private Resources mMockResources;

    private static final String PRICE_HISTORY_TITLE = "Price history across the web";

    private PriceInsightsBottomSheetMediator mPriceInsightsMediator;
    private PropertyModel mPropertyModel =
            new PropertyModel(PriceInsightsBottomSheetProperties.ALL_KEYS);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockResources).when(mMockContext).getResources();
        doReturn(PRICE_HISTORY_TITLE)
                .when(mMockResources)
                .getString(eq(R.string.price_history_title));
        mPriceInsightsMediator =
                new PriceInsightsBottomSheetMediator(
                        mMockContext, mMockShoppingService, mPropertyModel);
    }

    @Test
    public void testRequestShowContent() {
        mPriceInsightsMediator.requestShowContent();
        assertEquals(
                PRICE_HISTORY_TITLE,
                mPropertyModel.get(PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE));
    }
}
