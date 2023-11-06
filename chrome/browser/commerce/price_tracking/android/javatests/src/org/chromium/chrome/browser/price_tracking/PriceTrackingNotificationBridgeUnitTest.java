// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.NotificationData;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.Action;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeMessage;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification.NotificationDataType;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ExpandedView;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.PriceDropNotificationPayload;
import org.chromium.components.commerce.PriceTracking.ProductPrice;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;

/** Unit test for {@link PriceDropNotifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceTrackingNotificationBridgeUnitTest {
    private static final String TITLE = "title";
    private static final String TEXT = "text";
    private static final String ICON_URL = "http://www.example.com/icon";
    private static final String DESTINATION_URL = "http://www.example.com/destination";
    private static final String PRODUCT_NAME = "Awesome product";
    private static final long OFFER_ID = 10L;
    private static final long CURRENT_PRICE_AMOUNT_MICROS = 1000000L;
    private static final long PREVIOUS_PRICE_AMOUNT_MICROS = 2000000L;
    private static final String CURRENCY_CODE = "USD";
    private static final String ACTION_ID_0 = "visit_site";
    private static final String ACTION_ID_1 = "turn_off_alert";
    private static final String ACTION_TEXT_0 = "Visit site";
    private static final String ACTION_TEXT_1 = "Untrack price";

    private PriceTrackingNotificationBridge mPriceTrackingNotificationBridge;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock PriceDropNotifier mNotifier;
    @Mock PriceDropNotificationManager mPriceDropNotificationManager;

    @Captor ArgumentCaptor<PriceDropNotifier.NotificationData> mNotificationDataCaptor;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        CurrencyFormatter.Natives currencyFormatterJniMock =
                Mockito.mock(CurrencyFormatter.Natives.class);
        mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, currencyFormatterJniMock);
        Mockito.doReturn("$1.00")
                .when(currencyFormatterJniMock)
                .format(
                        Mockito.anyLong(),
                        Mockito.any(CurrencyFormatter.class),
                        Mockito.anyString());
        mPriceTrackingNotificationBridge =
                new PriceTrackingNotificationBridge(0, mNotifier, mPriceDropNotificationManager);
        when(mPriceDropNotificationManager.canPostNotification()).thenReturn(true);
    }

    // Creates a ChromeNotification.Builder that sets a valid ChromeNotification proto.
    private ChromeNotification.Builder createValidChromeNotification() {
        return createChromeNotification(
                createValidChromeMessage(), createValidPriceDropNotificationPayload());
    }

    // Create a ChromeNotification.Builder with specific ChromeMessage.
    private ChromeNotification.Builder createChromeNotification(
            ChromeMessage.Builder chromeMessage,
            PriceDropNotificationPayload.Builder priceDropNotificationPayload) {
        ChromeNotification.Builder builder = ChromeNotification.newBuilder();
        builder.setNotificationDataType(NotificationDataType.PRICE_DROP_NOTIFICATION);
        Any.Builder anyBuilder = Any.newBuilder();
        anyBuilder.setValue(priceDropNotificationPayload.build().toByteString());
        builder.setNotificationData(anyBuilder.build().toByteString());
        builder.setChromeMessage(chromeMessage.build());
        return builder;
    }

    private PriceDropNotificationPayload.Builder createValidPriceDropNotificationPayload() {
        PriceDropNotificationPayload.Builder priceDropPayload =
                PriceDropNotificationPayload.newBuilder();
        priceDropPayload.setProductName(PRODUCT_NAME);
        priceDropPayload.setOfferId(OFFER_ID);
        priceDropPayload.setDestinationUrl(DESTINATION_URL);
        ProductPrice.Builder currentPrice = ProductPrice.newBuilder();
        currentPrice.setAmountMicros(CURRENT_PRICE_AMOUNT_MICROS);
        currentPrice.setCurrencyCode(CURRENCY_CODE);
        priceDropPayload.setCurrentPrice(currentPrice);
        ProductPrice.Builder previousPrice = ProductPrice.newBuilder();
        previousPrice.setAmountMicros(PREVIOUS_PRICE_AMOUNT_MICROS);
        previousPrice.setCurrencyCode(CURRENCY_CODE);
        priceDropPayload.setPreviousPrice(previousPrice);
        return priceDropPayload;
    }

    // Create a valid ChromeMessage proto builder.
    private ChromeMessage.Builder createValidChromeMessage() {
        // Create ChromeMessage, some fields are duplicated to PriceDropNotificationPayload,
        ChromeMessage.Builder message = ChromeMessage.newBuilder();
        message.setDisplayTitle(TITLE);
        message.setDisplayText(TEXT);
        message.setIconImageUrl(ICON_URL);
        message.setDestinationUrl(DESTINATION_URL);
        ExpandedView.Builder expandedView = ExpandedView.newBuilder();
        expandedView.addAction(Action.newBuilder().setActionId(ACTION_ID_0).setText(ACTION_TEXT_0));
        expandedView.addAction(Action.newBuilder().setActionId(ACTION_ID_1).setText(ACTION_TEXT_1));
        message.setExpandedView(expandedView.build());
        return message;
    }

    @Test
    public void testShowNotification_ValidProto() {
        mPriceTrackingNotificationBridge.showNotification(
                createValidChromeNotification().build().toByteArray());
        verify(mNotifier).showNotification(mNotificationDataCaptor.capture());
        NotificationData data = mNotificationDataCaptor.getValue();

        // We are currently use client side strings, instead of the title/text in ChromeMessage
        // proto.
        Assert.assertEquals("$1.00 price drop on Awesome product", data.title);
        Assert.assertEquals("Now $1.00 on www.example.com", data.text);

        Assert.assertEquals(ICON_URL, data.iconUrl);
        Assert.assertEquals(DESTINATION_URL, data.destinationUrl);
        Assert.assertEquals(ACTION_ID_0, data.actions.get(0).actionId);
        Assert.assertEquals(ACTION_TEXT_0, data.actions.get(0).text);
        Assert.assertEquals(ACTION_ID_1, data.actions.get(1).actionId);
        Assert.assertEquals(ACTION_TEXT_1, data.actions.get(1).text);
        Assert.assertEquals(String.valueOf(OFFER_ID), data.offerId);
    }

    @Test
    public void testShowNotification_ChannelNotCreated() {
        when(mPriceDropNotificationManager.canPostNotification()).thenReturn(false);
        mPriceTrackingNotificationBridge.showNotification(
                createValidChromeNotification().build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_GarbageBytePayload() {
        mPriceTrackingNotificationBridge.showNotification(new byte[2]);
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_NoChromeMessage() {
        ChromeNotification.Builder builder = createValidChromeNotification();
        builder.clearChromeMessage();
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_NoDestinationURL() {
        PriceDropNotificationPayload.Builder priceDropNotificationPayload =
                createValidPriceDropNotificationPayload();
        priceDropNotificationPayload.clearDestinationUrl();
        ChromeNotification.Builder builder =
                createChromeNotification(createValidChromeMessage(), priceDropNotificationPayload);
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_InvalidDestinationURL() {
        PriceDropNotificationPayload.Builder priceDropNotificationPayload =
                createValidPriceDropNotificationPayload();
        priceDropNotificationPayload.setDestinationUrl("test");
        ChromeNotification.Builder builder =
                createChromeNotification(createValidChromeMessage(), priceDropNotificationPayload);
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_NoOfferId() {
        PriceDropNotificationPayload.Builder priceDropNotificationPayload =
                createValidPriceDropNotificationPayload();
        priceDropNotificationPayload.clearOfferId();
        ChromeNotification.Builder builder =
                createChromeNotification(createValidChromeMessage(), priceDropNotificationPayload);
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_PriceNotDrop() {
        PriceDropNotificationPayload.Builder priceDropNotificationPayload =
                createValidPriceDropNotificationPayload();
        // Make current price and previous price equal.
        priceDropNotificationPayload.setPreviousPrice(
                priceDropNotificationPayload.getCurrentPrice());
        ChromeNotification.Builder builder =
                createChromeNotification(createValidChromeMessage(), priceDropNotificationPayload);
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }
}
