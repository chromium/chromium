// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerImpl;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.Message;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link MessageCardProviderMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageCardProviderMediatorUnitTest {
    private static final int TESTING_ACTION = -1;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private MessageCardProviderMediator<@MessageType Integer, @UiType Integer> mMediator;

    @Mock private ServiceDismissActionProvider<@MessageType Integer> mServiceDismissActionProvider;

    @Mock private Context mContext;

    @Mock private Resources mResourcesMock;

    @Mock private PriceMessageService.PriceMessageData mPriceMessageData;

    @Mock private IphMessageService.IphMessageData mIphMessageData;

    @Mock
    private IncognitoReauthPromoMessageService.IncognitoReauthMessageData
            mIncognitoReauthMessageData;

    @Before
    public void setUp() {
        doNothing().when(mServiceDismissActionProvider).dismiss(anyInt());
        mMediator = new MessageCardProviderMediator<>(mContext, mServiceDismissActionProvider);
        mMediator.addMessageService(initService(MessageType.FOR_TESTING));
        mMediator.addMessageService(initService(MessageType.PRICE_MESSAGE));
        mMediator.addMessageService(initService(MessageType.IPH));
        mMediator.addMessageService(initService(MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE));
    }

    private void enqueueMessageItem(@MessageType int type, int tabSuggestionAction) {
        switch (type) {
            case MessageType.PRICE_MESSAGE:
                when(mPriceMessageData.getPriceDrop()).thenReturn(null);
                when(mPriceMessageData.getDismissActionProvider()).thenReturn(() -> {});
                when(mPriceMessageData.getAcceptActionProvider()).thenReturn(() -> {});
                when(mPriceMessageData.getType()).thenReturn(PriceMessageType.PRICE_WELCOME);
                mMediator.messageReady(
                        type,
                        (a, b) ->
                                PriceMessageCardViewModel.create(
                                        a,
                                        b,
                                        mPriceMessageData,
                                        new PriceDropNotificationManagerImpl(mock())));
                break;
            case MessageType.IPH:
                when(mIphMessageData.getDismissActionProvider()).thenReturn(() -> {});
                when(mIphMessageData.getAcceptActionProvider()).thenReturn(() -> {});
                mMediator.messageReady(
                        type, (a, b) -> IphMessageCardViewModel.create(a, b, mIphMessageData));
                break;
            case MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE:
                when(mIncognitoReauthMessageData.getAcceptActionProvider()).thenReturn(() -> {});
                mMediator.messageReady(
                        type,
                        (a, b) ->
                                IncognitoReauthPromoViewModel.create(
                                        a, b, mIncognitoReauthMessageData));
                break;
            default:
                mMediator.messageReady(
                        type, (a, b) -> new PropertyModel(MessageCardViewProperties.ALL_KEYS));
        }
    }

    @Test
    public void getNextMessageItemForTypeTest() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        assertNotNull(mMediator.getNextMessageItemForType(MessageType.FOR_TESTING));
        assertTrue(getMessageItemsForService(MessageType.FOR_TESTING).isEmpty());
        assertNotNull(getShownMessageFromService(MessageType.FOR_TESTING));
    }

    @Test
    public void getNextMessageItemForTypeTest_TwoDifferentTypeMessage() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        assertNotNull(mMediator.getNextMessageItemForType(MessageType.PRICE_MESSAGE));
        assertTrue(getMessageItemsForService(MessageType.PRICE_MESSAGE).isEmpty());
        assertNotNull(getShownMessageFromService(MessageType.PRICE_MESSAGE));

        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        assertNotNull(mMediator.getNextMessageItemForType(MessageType.PRICE_MESSAGE));
        assertNotNull(mMediator.getNextMessageItemForType(MessageType.FOR_TESTING));
        assertTrue(getMessageItemsForService(MessageType.FOR_TESTING).isEmpty());
        assertNotNull(getShownMessageFromService(MessageType.FOR_TESTING));
    }

    @Test
    public void getNextMessageItemForTypeTest_OneMessageForEachMessageType() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        Message<@MessageType Integer> priceMessage =
                mMediator.getNextMessageItemForType(MessageType.PRICE_MESSAGE);
        Message<@MessageType Integer> testMessage =
                mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);

        assertNotNull(priceMessage);
        assertNotNull(testMessage);

        assertNotNull(getShownMessageFromService(MessageType.PRICE_MESSAGE));
        assertNotNull(getShownMessageFromService(MessageType.FOR_TESTING));
    }

    @Test
    public void getNextMessageItemForTypeTest_ReturnFirstMessageFromMultipleSameTypeMessages() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages =
                getMessageItemsForService(MessageType.FOR_TESTING);
        assertEquals(2, messages.size());
        final Message<@MessageType Integer> testingMessage1 = messages.get(0);
        final Message<@MessageType Integer> testingMessage2 = messages.get(1);

        Message<@MessageType Integer> message =
                mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
        assertEquals(testingMessage1, message);

        assertNotNull(getShownMessageFromService(MessageType.FOR_TESTING));
        assertEquals(testingMessage1, getShownMessageFromService(MessageType.FOR_TESTING));

        assertEquals(1, getMessageItemsForService(MessageType.FOR_TESTING).size());
        assertEquals(testingMessage2, getMessageItemsForService(MessageType.FOR_TESTING).get(0));
    }

    @Test
    public void getNextMessageItemForTypeTest_PersistUntilInvalidationOccurred() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages =
                getMessageItemsForService(MessageType.FOR_TESTING);
        assertEquals(2, messages.size());
        final Message<@MessageType Integer> testingMessage1 = messages.get(0);

        // Test message is persisted.
        for (int i = 0; i < 2; i++) {
            Message<@MessageType Integer> message =
                    mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
            assertEquals(testingMessage1, message);
        }

        // Test message updated after invalidation, and the updated message is persisted.
        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);
        Message<@MessageType Integer> newMessage =
                mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
        Assert.assertNotEquals(testingMessage1, newMessage);
        for (int i = 0; i < 2; i++) {
            Message<@MessageType Integer> message =
                    mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
            assertEquals(newMessage, message);
        }
    }

    @Test
    public void getNextMessageItemForTypeTest_ReturnNextMessageIfShownMessageIsInvalided() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages =
                getMessageItemsForService(MessageType.FOR_TESTING);
        assertEquals(2, messages.size());
        final Message<@MessageType Integer> testingMessage1 = messages.get(0);
        final Message<@MessageType Integer> testingMessage2 = messages.get(1);

        Message<@MessageType Integer> message =
                mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
        assertEquals(testingMessage1, message);

        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);

        message = mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
        assertEquals(testingMessage2, message);

        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);
        Assert.assertNull(mMediator.getNextMessageItemForType(MessageType.FOR_TESTING));
    }

    @Test
    public void invalidate_allMessages() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        mMediator.messageInvalidate(MessageType.PRICE_MESSAGE);

        Assert.assertNull(getShownMessageFromService(MessageType.PRICE_MESSAGE));
        assertTrue(getMessageItemsForService(MessageType.PRICE_MESSAGE).isEmpty());

        // Testing multiple Messages has the same type.
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        mMediator.messageInvalidate(MessageType.FOR_TESTING);
        Assert.assertNull(getShownMessageFromService(MessageType.FOR_TESTING));
        assertTrue(getMessageItemsForService(MessageType.FOR_TESTING).isEmpty());
    }

    @Test
    public void invalidate_shownMessage() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        mMediator.getNextMessageItemForType(MessageType.PRICE_MESSAGE);
        mMediator.invalidateShownMessage(MessageType.PRICE_MESSAGE);

        verify(mServiceDismissActionProvider).dismiss(anyInt());
        Assert.assertNull(getShownMessageFromService(MessageType.PRICE_MESSAGE));
        assertTrue(getMessageItemsForService(MessageType.PRICE_MESSAGE).isEmpty());

        // Testing multiple Messages has the same type.
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        mMediator.getNextMessageItemForType(MessageType.FOR_TESTING);
        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);
        Assert.assertNull(getShownMessageFromService(MessageType.FOR_TESTING));
        assertFalse(getMessageItemsForService(MessageType.FOR_TESTING).isEmpty());
    }

    @Test
    public void buildModel_ForPriceMessage() {
        String titleText = "Price drop spotted";
        doReturn(titleText).when(mContext).getString(R.string.price_drop_spotted_title);

        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        PropertyModel model = getMessageItemsForService(MessageType.PRICE_MESSAGE).get(0).model;
        assertEquals(MessageType.PRICE_MESSAGE, model.get(MessageCardViewProperties.MESSAGE_TYPE));
        assertEquals(titleText, model.get(MessageCardViewProperties.TITLE_TEXT));
    }

    @Test
    public void buildModel_ForIphMessage() {
        enqueueMessageItem(MessageType.IPH, -1);

        PropertyModel model = getMessageItemsForService(MessageType.IPH).get(0).model;
        assertEquals(MessageType.IPH, model.get(MessageCardViewProperties.MESSAGE_TYPE));
    }

    @Test
    public void buildModel_ForIncognitoReauthPromoMessage() {
        final int height = 1;
        final int width = 2;
        when(mResourcesMock.getDimensionPixelSize(
                        R.dimen.incognito_reauth_promo_message_icon_height))
                .thenReturn(height);
        when(mResourcesMock.getDimensionPixelSize(
                        R.dimen.incognito_reauth_promo_message_icon_width))
                .thenReturn(width);
        when(mContext.getResources()).thenReturn(mResourcesMock);

        enqueueMessageItem(MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE, -1);

        PropertyModel model =
                getMessageItemsForService(MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE).get(0).model;
        assertEquals(
                MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                model.get(MessageCardViewProperties.MESSAGE_TYPE));
        verify(mResourcesMock, times(1))
                .getDimensionPixelSize(R.dimen.incognito_reauth_promo_message_icon_height);
        verify(mResourcesMock, times(1))
                .getDimensionPixelSize(R.dimen.incognito_reauth_promo_message_icon_width);
        verify(mContext, times(2)).getResources();
    }

    @Test
    public void isMessageShownTest() {
        assertFalse(
                mMediator.isMessageShown(
                        MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME));
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);
        // Mock pulling this message, which will move the message from mMessageItems to
        // mShownMessageItems.
        mMediator.getNextMessageItemForType(MessageType.PRICE_MESSAGE);
        assertTrue(
                mMediator.isMessageShown(
                        MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME));
    }

    private List<Message<@MessageType Integer>> getMessageItemsForService(
            @MessageType int messageType) {
        return mMediator.getMessageServicesMap().get(messageType).getMessageItems();
    }

    @Nullable
    private Message<@MessageType Integer> getShownMessageFromService(@MessageType int messageType) {
        return mMediator.getMessageServicesMap().get(messageType).getShownMessage();
    }

    private static MessageService<@MessageType Integer, @UiType Integer> initService(
            @MessageType int messageType) {
        return new MessageService<>(
                messageType,
                UiType.IPH_MESSAGE,
                R.layout.tab_grid_message_card_item,
                MessageCardViewBinder::bind);
    }
}
