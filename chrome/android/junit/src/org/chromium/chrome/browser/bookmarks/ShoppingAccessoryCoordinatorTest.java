// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.ShoppingAccessoryViewProperties.PriceInfo;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ShoppingAccessoryCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShoppingAccessoryCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private CurrencyFormatter.Natives mCurrencyFormatterJniMock;
    @Mock private ShoppingService mShoppingService;

    Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, mCurrencyFormatterJniMock);
    }

    @Test
    public void testSetupAndSetModel() {
        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setCurrentPrice(
                                ProductPrice.newBuilder()
                                        .setCurrencyCode("USD")
                                        .setAmountMicros(100)
                                        .build())
                        .setPreviousPrice(
                                ProductPrice.newBuilder()
                                        .setCurrencyCode("USD")
                                        .setAmountMicros(100)
                                        .build())
                        .build();
        ShoppingAccessoryCoordinator coordinator =
                new ShoppingAccessoryCoordinator(mActivity, specifics, mShoppingService);
        Assert.assertNotNull(coordinator.getView());

        PropertyModel model = coordinator.getModel();
        Assert.assertEquals(model.get(ShoppingAccessoryViewProperties.PRICE_TRACKED), true);

        PriceInfo info = model.get(ShoppingAccessoryViewProperties.PRICE_INFO);
        Assert.assertEquals(false, info.isPriceDrop());
    }

    @Test
    public void testSetPriceTrackingEnabled() {
        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setCurrentPrice(
                                ProductPrice.newBuilder()
                                        .setCurrencyCode("USD")
                                        .setAmountMicros(100)
                                        .build())
                        .setPreviousPrice(
                                ProductPrice.newBuilder()
                                        .setCurrencyCode("USD")
                                        .setAmountMicros(100)
                                        .build())
                        .build();
        ShoppingAccessoryCoordinator coordinator =
                new ShoppingAccessoryCoordinator(mActivity, specifics, mShoppingService);

        PropertyModel model = coordinator.getModel();
        Assert.assertTrue(model.get(ShoppingAccessoryViewProperties.PRICE_TRACKED));

        coordinator.setPriceTrackingEnabled(false);
        Assert.assertFalse(model.get(ShoppingAccessoryViewProperties.PRICE_TRACKED));
    }

    @Test
    public void testPriceDrop() {
        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setCurrentPrice(
                                ProductPrice.newBuilder()
                                        .setCurrencyCode("USD")
                                        .setAmountMicros(50)
                                        .build())
                        .setPreviousPrice(
                                ProductPrice.newBuilder()
                                        .setCurrencyCode("USD")
                                        .setAmountMicros(100)
                                        .build())
                        .build();
        ShoppingAccessoryCoordinator coordinator =
                new ShoppingAccessoryCoordinator(mActivity, specifics, mShoppingService);

        PriceInfo info = coordinator.getModel().get(ShoppingAccessoryViewProperties.PRICE_INFO);
        Assert.assertEquals(true, info.isPriceDrop());
    }
}
