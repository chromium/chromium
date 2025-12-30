// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;

/** Unit tests for {@link PriceMessageService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceMessageServiceUnitTest {
    private static final int BINDING_TAB_ID = 456;
    private static final int INITIAL_SHOW_COUNT = 0;
    private static final int MAX_SHOW_COUNT = 20;
    private static final String PRICE = "$300";
    private static final String PREVIOUS_PRICE = "$400";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Context mContext;
    @Mock PriceMessageService.PriceWelcomeMessageProvider mMessageProvider;
    @Mock PriceMessageService.PriceWelcomeMessageReviewActionProvider mReviewActionProvider;
    @Mock ServiceDismissActionProvider<@MessageType Integer> mServiceDismissActionProvider;
    @Mock Profile mProfile;

    private PriceMessageService mMessageService;
    private PriceTabData mPriceTabData;

    @Before
    public void setUp() {

        mPriceTabData =
                new PriceTabData(
                        BINDING_TAB_ID,
                        new ShoppingPersistedTabData.PriceDrop(PRICE, PREVIOUS_PRICE));

        FeatureOverrides.enable(ChromeFeatureList.PRICE_ANNOTATIONS);

        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeInt(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, INITIAL_SHOW_COUNT);
        assertTrue(PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile));

        mMessageService =
                new PriceMessageService(
                        mContext, mProfile, () -> mMessageProvider, () -> mReviewActionProvider);
        mMessageService.initialize(mServiceDismissActionProvider);
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
        mMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, mPriceTabData);
        verify(mServiceDismissActionProvider, times(1)).dismiss(eq(MessageType.PRICE_MESSAGE));
        assertEquals(mPriceTabData, mMessageService.getPriceTabDataForTesting());
        assertFalse(mMessageService.getMessageItems().isEmpty());
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

        mMessageService.dismiss();
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
        verify(mServiceDismissActionProvider, times(1)).dismiss(eq(MessageType.PRICE_MESSAGE));
        mMessageService.invalidateMessage();
        assertNull(mMessageService.getPriceTabDataForTesting());
        verify(mServiceDismissActionProvider, times(2)).dismiss(eq(MessageType.PRICE_MESSAGE));
    }
}
