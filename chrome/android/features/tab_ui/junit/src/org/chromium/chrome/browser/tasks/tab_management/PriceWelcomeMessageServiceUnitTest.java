// Copyright 2020 The Chromium Authors. All rights reserved.
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceWelcomeMessageService.PriceTabData;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Unit tests for {@link PriceWelcomeMessageService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceWelcomeMessageServiceUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int BINDING_TAB_ID = 456;
    private static final int INITIAL_SHOW_COUNT = 0;
    private static final int MAX_SHOW_COUNT = 20;

    private static final String PRICE = "$300";
    private static final String PREVIOUS_PRICE = "$400";

    @Mock
    PriceWelcomeMessageService.PriceWelcomeMessageProvider mMessageProvider;
    @Mock
    PriceWelcomeMessageService.PriceWelcomeMessageReviewActionProvider mReviewActionProvider;
    @Mock
    MessageService.MessageObserver mMessageObserver;

    private PriceWelcomeMessageService mMessageService;
    private PriceTabData mPriceTabData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPriceTabData = new PriceTabData(
                BINDING_TAB_ID, new ShoppingPersistedTabData.PriceDrop(PRICE, PREVIOUS_PRICE));

        doReturn(mPriceTabData).when(mMessageProvider).getFirstTabShowingPriceCard();
        doNothing().when(mMessageObserver).messageReady(anyInt(), any());
        doNothing().when(mMessageObserver).messageInvalidate(anyInt());

        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeInt(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, INITIAL_SHOW_COUNT);
        assertFalse(PriceTrackingUtilities.isPriceWelcomeMessageCardDisabled());

        mMessageService = new PriceWelcomeMessageService(mMessageProvider, mReviewActionProvider);
        mMessageService.addObserver(mMessageObserver);
    }

    @Test
    public void testPrepareMessage_messageCardDisabled() {
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, false);
        mMessageService.preparePriceMessage();
        verify(mMessageProvider, times(0)).getFirstTabShowingPriceCard();
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        mMessageService.preparePriceMessage();
        verify(mMessageProvider, times(1)).getFirstTabShowingPriceCard();
    }

    @Test
    public void testPrepareMessage_noTabShowingPriceCard() {
        doReturn(null).when(mMessageProvider).getFirstTabShowingPriceCard();
        mMessageService.preparePriceMessage();
        assertNull(mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
        assertEquals(
                INITIAL_SHOW_COUNT, PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount());
    }

    @Test
    public void testPrepareMessage_exceedMaxShowCount() {
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeInt(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, MAX_SHOW_COUNT);
        mMessageService.preparePriceMessage();
        assertEquals(
                MAX_SHOW_COUNT + 1, PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount());
        assertNull(mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
        assertTrue(PriceTrackingUtilities.isPriceWelcomeMessageCardDisabled());
    }

    @Test
    public void testPrepareMessage_hasTabShowingPriceCard() {
        doReturn(mPriceTabData).when(mMessageProvider).getFirstTabShowingPriceCard();
        InOrder inOrder = Mockito.inOrder(mMessageObserver);
        mMessageService.preparePriceMessage();
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());
        inOrder.verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
        inOrder.verify(mMessageObserver, times(1))
                .messageReady(eq(MessageService.MessageType.PRICE_WELCOME),
                        any(PriceWelcomeMessageService.PriceWelcomeMessageData.class));

        // We sendAvailabilityNotification only if the newly obtained priceTabData is different from
        // currently existing priceTabData.
        mMessageService.preparePriceMessage();
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.PRICE_WELCOME),
                        any(PriceWelcomeMessageService.PriceWelcomeMessageData.class));

        PriceTabData priceTabData = new PriceTabData(
                BINDING_TAB_ID + 1, new ShoppingPersistedTabData.PriceDrop(PRICE, PREVIOUS_PRICE));
        doReturn(priceTabData).when(mMessageProvider).getFirstTabShowingPriceCard();
        mMessageService.preparePriceMessage();
        assertEquals(priceTabData, mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(2)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
        verify(mMessageObserver, times(2))
                .messageReady(eq(MessageType.PRICE_WELCOME),
                        any(PriceWelcomeMessageService.PriceWelcomeMessageData.class));
    }

    @Test
    public void testReview() {
        int index = 1;
        doReturn(index).when(mMessageProvider).getTabIndexFromTabId(BINDING_TAB_ID);
        doNothing().when(mReviewActionProvider).scrollToBindingTab(anyInt());
        mMessageService.preparePriceMessage();
        mMessageService.review();
        verify(mReviewActionProvider).scrollToBindingTab(index);
        assertTrue(PriceTrackingUtilities.isPriceWelcomeMessageCardDisabled());
    }

    @Test
    public void testDismiss() {
        mMessageService.dismiss();
        assertTrue(PriceTrackingUtilities.isPriceWelcomeMessageCardDisabled());
    }

    @Test
    public void testGetBindingTabId() {
        assertEquals(Tab.INVALID_TAB_ID, mMessageService.getBindingTabId());
        mMessageService.preparePriceMessage();
        assertEquals(BINDING_TAB_ID, mMessageService.getBindingTabId());
    }

    @Test
    public void testInvalidateMessage() {
        mMessageService.preparePriceMessage();
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(1)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
        mMessageService.invalidateMessage();
        assertNull(mMessageService.getPriceTabDataForTesting());
        verify(mMessageObserver, times(2)).messageInvalidate(eq(MessageType.PRICE_WELCOME));
    }
}
