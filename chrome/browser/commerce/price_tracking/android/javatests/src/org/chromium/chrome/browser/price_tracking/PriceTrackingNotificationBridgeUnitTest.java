// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.NotificationData;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.Action;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeMessage;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification.NotificationDataType;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ExpandedView;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.PriceDropNotificationPayload;

/**
 * Unit test for {@link PriceDropNotifier}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceTrackingNotificationBridgeUnitTest {
    private static final String TITLE = "title";
    private static final String TEXT = "text";
    private static final String ICON_URL = "http://www.example.com/icon";
    private static final String DESTINATION_URL = "http://www.example.com/destination";
    private static final long OFFER_ID = 10L;
    private static final String ACTION_ID_0 = "action_id_0";
    private static final String ACTION_ID_1 = "action_id_1";
    private static final String ACTION_TEXT_0 = "action_text_0";
    private static final String ACTION_TEXT_1 = "action_text_1";

    private PriceTrackingNotificationBridge mPriceTrackingNotificationBridge;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    PriceDropNotifier mNotifier;

    @Captor
    ArgumentCaptor<PriceDropNotifier.NotificationData> mNotificationDataCaptor;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mPriceTrackingNotificationBridge = new PriceTrackingNotificationBridge(0, mNotifier);
    }

    // Creates a ChromeNotification.Builder that sets a valid ChromeNotification proto.
    private ChromeNotification.Builder createValidPayload() {
        return createPayloadWithChromeMessage(createValidChromeMessage());
    }

    // Create a ChromeNotification.Builder with specific ChromeMessage.
    private ChromeNotification.Builder createPayloadWithChromeMessage(
            ChromeMessage.Builder chromeMessage) {
        ChromeNotification.Builder builder = ChromeNotification.newBuilder();
        builder.setNotificationDataType(NotificationDataType.PRICE_DROP_NOTIFICATION);

        // Only offer id is used in unido code path.
        PriceDropNotificationPayload.Builder priceDropPayload =
                PriceDropNotificationPayload.newBuilder();
        priceDropPayload.setOfferId(OFFER_ID);
        builder.setNotificationData(priceDropPayload.build().toByteString());
        builder.setChromeMessage(chromeMessage.build());
        return builder;
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
                createValidPayload().build().toByteArray());
        verify(mNotifier).showNotification(mNotificationDataCaptor.capture());
        NotificationData data = mNotificationDataCaptor.getValue();
        Assert.assertEquals(TITLE, data.title);
        Assert.assertEquals(TEXT, data.text);
        Assert.assertEquals(ICON_URL, data.iconUrl);
        Assert.assertEquals(DESTINATION_URL, data.destinationUrl);
        Assert.assertEquals(ACTION_ID_0, data.actions.get(0).actionId);
        Assert.assertEquals(ACTION_TEXT_0, data.actions.get(0).text);
        Assert.assertEquals(ACTION_ID_1, data.actions.get(1).actionId);
        Assert.assertEquals(ACTION_TEXT_1, data.actions.get(1).text);
        Assert.assertEquals(String.valueOf(OFFER_ID), data.offerId);
    }

    @Test
    public void testShowNotification_NoChromeMessage() {
        ChromeNotification.Builder builder = createValidPayload();
        builder.clearChromeMessage();
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }

    @Test
    public void testShowNotification_NoDestinationURL() {
        ChromeMessage.Builder chromeMessage = createValidChromeMessage();
        chromeMessage.clearDestinationUrl();
        ChromeNotification.Builder builder = createPayloadWithChromeMessage(chromeMessage);
        mPriceTrackingNotificationBridge.showNotification(builder.build().toByteArray());
        verify(mNotifier, times(0)).showNotification(any());
    }
}
