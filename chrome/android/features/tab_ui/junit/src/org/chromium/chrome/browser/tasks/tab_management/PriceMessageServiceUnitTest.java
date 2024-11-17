// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;

/** Unit tests for {@link PriceMessageService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceMessageServiceUnitTest {

    private static final int BINDING_TAB_ID = 456;
    private static final int INITIAL_SHOW_COUNT = 0;
    private static final int MAX_SHOW_COUNT = 20;

    private static final String PRICE = "$300";
    private static final String PREVIOUS_PRICE = "$400";

    @Mock PriceMessageService.PriceWelcomeMessageProvider mMessageProvider;
    @Mock PriceMessageService.PriceWelcomeMessageReviewActionProvider mReviewActionProvider;
    @Mock MessageService.MessageObserver mMessageObserver;
    @Mock PriceDropNotificationManager mNotificationManager;
    @Mock Profile mProfile;

    private PriceMessageService mMessageService;
    private PriceTabData mPriceTabData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPriceTabData =
                new PriceTabData(
                        BINDING_TAB_ID,
                        new ShoppingPersistedTabData.PriceDrop(PRICE, PREVIOUS_PRICE));

        doNothing().when(mMessageObserver).messageReady(anyInt(), any());
        doNothing().when(mMessageObserver).messageInvalidate(anyInt());

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        FeatureList.setTestValues(testValues);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeInt(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, INITIAL_SHOW_COUNT);
        assertTrue(PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile));

        mMessageService =
                new PriceMessageService(
                        mProfile,
                        () -> mMessageProvider,
                        () -> mReviewActionProvider,
                        mNotificationManager);
        mMessageService.addObserver(mMessageObserver);
    }

    @Test(expected = AssertionError.class)
    public void testPrepareMessage_PriceWelcome_MessageDisabled() {
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, false);
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
    }

    @Test
    public void testPrepareMessage_PriceWelcome_ExceedMaxShowCount() {
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeInt(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, MAX_SHOW_COUNT);
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        assertEquals(
                MAX_SHOW_COUNT + 1, PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount());
        assertFalse(PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile));
    }

    @Test
    public void testPrepareMessage_PriceWelcome() {
        InOrder inOrder = Mockito.inOrder(mMessageObserver);
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        inOrder.verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_MESSAGE));
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());
        inOrder.verify(mMessageObserver, times(1))
                .messageReady(
                        eq(MessageService.MessageType.PRICE_MESSAGE),
                        any(PriceMessageService.PriceMessageData.class));
        assertEquals(
                INITIAL_SHOW_COUNT + 1,
                PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount());
    }

    @Test
    public void testReview_PriceWelcome() {
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());

        int index = 1;
        doReturn(index).when(mMessageProvider).getTabIndexFromTabId(BINDING_TAB_ID);
        doNothing().when(mReviewActionProvider).scrollToTab(anyInt());
        mMessageService.review(PriceMessageType.PRICE_WELCOME);
        verify(mReviewActionProvider).scrollToTab(index);
        verify(mMessageProvider).showPriceDropTooltip(index);
        assertFalse(PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile));
        assertNull(mMessageService.getPriceTabDataForTesting());
    }

    @Test
    public void testDismiss_PriceWelcome() {
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());

        mMessageService.dismiss(PriceMessageType.PRICE_WELCOME);
        assertFalse(PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile));
        assertNull(mMessageService.getPriceTabDataForTesting());
    }

    @Test
    public void testGetBindingTabId() {
        assertEquals(Tab.INVALID_TAB_ID, mMessageService.getBindingTabId());
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        assertEquals(BINDING_TAB_ID, mMessageService.getBindingTabId());
    }

    @Test
    public void testInvalidateMessage() {
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_MESSAGE));
        mMessageService.invalidateMessage();
        assertNull(mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(2)).messageInvalidate(eq(MessageType.PRICE_MESSAGE));
    }
}
