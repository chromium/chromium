// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardProviderMediator.Message;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link MessageCardProviderMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageCardProviderMediatorUnitTest {
    private static final int TESTING_ACTION = -1;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private MessageCardProviderMediator<@MessageType Integer> mMediator;

    @Mock private ServiceDismissActionProvider<@MessageType Integer> mServiceDismissActionProvider;

    @Mock private Context mContext;

    @Mock private Resources mResourcesMock;

    @Mock private Profile mProfileMock;
    @Mock private Profile mIncognitoProfileMock;

    @Mock private PriceMessageService.PriceMessageData mPriceMessageData;

    @Mock private Supplier<Profile> mProfileSupplier;

    @Mock private IphMessageService.IphMessageData mIphMessageData;

    @Mock
    private IncognitoReauthPromoMessageService.IncognitoReauthMessageData
            mIncognitoReauthMessageData;

    @Before
    public void setUp() {

        doReturn(true).when(mIncognitoProfileMock).isOffTheRecord();
        doReturn(mProfileMock).when(mProfileSupplier).get();
        doNothing().when(mServiceDismissActionProvider).dismiss(anyInt());
        mMediator =
                new MessageCardProviderMediator<>(
                        mContext, mProfileSupplier, mServiceDismissActionProvider);
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
                                        new PriceDropNotificationManagerImpl(mProfileMock)));
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
                mMediator.messageReady(type, (a, b) -> new PropertyModel());
        }
    }

    @Test
    public void getMessageItemsTest() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void getMessageItemsTest_TwoDifferentTypeMessage() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());

        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        Assert.assertEquals(2, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void getMessageItemsTest_OneMessageForEachMessageType() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages = mMediator.getMessageItems();
        Assert.assertEquals(2, messages.size());
        Assert.assertEquals(MessageType.PRICE_MESSAGE, (int) messages.get(0).type);
        Assert.assertEquals(MessageType.FOR_TESTING, (int) messages.get(1).type);

        Assert.assertEquals(2, mMediator.getShownMessageItemsForTesting().size());
        Assert.assertTrue(
                mMediator.getShownMessageItemsForTesting().containsKey(MessageType.PRICE_MESSAGE));
        Assert.assertTrue(
                mMediator.getShownMessageItemsForTesting().containsKey(MessageType.FOR_TESTING));
    }

    @Test
    public void getMessageItemsTest_ReturnFirstMessageFromMultipleSameTypeMessages() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages =
                mMediator.getReadyMessageItemsForTesting().get(MessageType.FOR_TESTING);
        Assert.assertEquals(2, messages.size());
        final Message<@MessageType Integer> testingMessage1 = messages.get(0);
        final Message<@MessageType Integer> testingMessage2 = messages.get(1);

        messages = mMediator.getMessageItems();
        Assert.assertEquals(1, messages.size());
        Assert.assertEquals(testingMessage1, messages.get(0));

        Assert.assertEquals(1, mMediator.getShownMessageItemsForTesting().size());
        Assert.assertEquals(
                testingMessage1,
                mMediator.getShownMessageItemsForTesting().get(MessageType.FOR_TESTING));

        Assert.assertEquals(1, mMediator.getShownMessageItemsForTesting().size());
        Assert.assertEquals(
                testingMessage2,
                mMediator.getReadyMessageItemsForTesting().get(MessageType.FOR_TESTING).get(0));
    }

    @Test
    public void getMessageItemsTest_PersistUntilInvalidationOccurred() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages =
                mMediator.getReadyMessageItemsForTesting().get(MessageType.FOR_TESTING);
        Assert.assertEquals(2, messages.size());
        final Message<@MessageType Integer> testingMessage1 = messages.get(0);

        // Test message is persisted.
        for (int i = 0; i < 2; i++) {
            messages = mMediator.getMessageItems();
            Assert.assertEquals(1, messages.size());
            Assert.assertEquals(testingMessage1, messages.get(0));
        }

        // Test message updated after invalidation, and the updated message is persisted.
        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);
        messages = mMediator.getMessageItems();
        final Message<@MessageType Integer> newMessage = messages.get(0);
        Assert.assertEquals(1, messages.size());
        Assert.assertNotEquals(testingMessage1, newMessage);
        for (int i = 0; i < 2; i++) {
            messages = mMediator.getMessageItems();
            Assert.assertEquals(1, messages.size());
            Assert.assertEquals(newMessage, messages.get(0));
        }
    }

    @Test
    public void getMessageItemsTest_ReturnNextMessageIfShownMessageIsInvalided() {
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        List<Message<@MessageType Integer>> messages =
                mMediator.getReadyMessageItemsForTesting().get(MessageType.FOR_TESTING);
        Assert.assertEquals(2, messages.size());
        final Message<@MessageType Integer> testingMessage1 = messages.get(0);
        final Message<@MessageType Integer> testingMessage2 = messages.get(1);

        messages = mMediator.getMessageItems();
        Assert.assertEquals(1, messages.size());
        Assert.assertEquals(testingMessage1, messages.get(0));

        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);

        messages = mMediator.getMessageItems();
        Assert.assertEquals(1, messages.size());
        Assert.assertEquals(testingMessage2, messages.get(0));

        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);
        Assert.assertEquals(0, mMediator.getMessageItems().size());
    }

    @Test
    public void invalidate_allMessages() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        mMediator.messageInvalidate(MessageType.PRICE_MESSAGE);

        Assert.assertFalse(
                mMediator.getReadyMessageItemsForTesting().containsKey(MessageType.PRICE_MESSAGE));
        Assert.assertFalse(
                mMediator.getShownMessageItemsForTesting().containsKey(MessageType.PRICE_MESSAGE));

        // Testing multiple Messages has the same type.
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        mMediator.messageInvalidate(MessageType.FOR_TESTING);
        Assert.assertFalse(
                mMediator.getReadyMessageItemsForTesting().containsKey(MessageType.FOR_TESTING));
        Assert.assertFalse(
                mMediator.getShownMessageItemsForTesting().containsKey(MessageType.FOR_TESTING));
    }

    @Test
    public void invalidate_shownMessage() {
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        mMediator.getMessageItems();
        mMediator.invalidateShownMessage(MessageType.PRICE_MESSAGE);

        verify(mServiceDismissActionProvider).dismiss(anyInt());
        Assert.assertFalse(
                mMediator.getShownMessageItemsForTesting().containsKey(MessageType.PRICE_MESSAGE));
        Assert.assertFalse(
                mMediator.getReadyMessageItemsForTesting().containsKey(MessageType.PRICE_MESSAGE));

        // Testing multiple Messages has the same type.
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageType.FOR_TESTING, TESTING_ACTION);

        mMediator.getMessageItems();
        mMediator.invalidateShownMessage(MessageType.FOR_TESTING);
        Assert.assertFalse(
                mMediator.getShownMessageItemsForTesting().containsKey(MessageType.FOR_TESTING));
        Assert.assertTrue(
                mMediator.getReadyMessageItemsForTesting().containsKey(MessageType.FOR_TESTING));
    }

    @Test
    public void buildModel_ForPriceMessage() {
        String titleText = "Price drop spotted";
        doReturn(titleText).when(mContext).getString(R.string.price_drop_spotted_title);

        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);

        PropertyModel model =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageType.PRICE_MESSAGE)
                        .get(0)
                        .model;
        Assert.assertEquals(
                MessageType.PRICE_MESSAGE, model.get(MessageCardViewProperties.MESSAGE_TYPE));
        Assert.assertEquals(titleText, model.get(MessageCardViewProperties.TITLE_TEXT));
    }

    @Test
    public void buildModel_ForIphMessage() {
        enqueueMessageItem(MessageType.IPH, -1);

        PropertyModel model =
                mMediator.getReadyMessageItemsForTesting().get(MessageType.IPH).get(0).model;
        Assert.assertEquals(MessageType.IPH, model.get(MessageCardViewProperties.MESSAGE_TYPE));
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
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE)
                        .get(0)
                        .model;
        Assert.assertEquals(
                MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                model.get(MessageCardViewProperties.MESSAGE_TYPE));
        verify(mResourcesMock, times(1))
                .getDimensionPixelSize(R.dimen.incognito_reauth_promo_message_icon_height);
        verify(mResourcesMock, times(1))
                .getDimensionPixelSize(R.dimen.incognito_reauth_promo_message_icon_width);
        verify(mContext, times(2)).getResources();
    }

    @Test
    public void getMessageItemsTest_UpdateIncognito() {
        enqueueMessageItem(MessageType.IPH, -1);

        PropertyModel messageModel = mMediator.getMessageItems().get(0).model;
        Assert.assertFalse(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));

        doReturn(mIncognitoProfileMock).when(mProfileSupplier).get();
        messageModel = mMediator.getMessageItems().get(0).model;
        Assert.assertTrue(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));
    }

    @Test
    public void getNextMessageItemForTypeTest_UpdateIncognito() {
        enqueueMessageItem(MessageType.IPH, -1);

        PropertyModel messageModel = mMediator.getNextMessageItemForType(MessageType.IPH).model;
        Assert.assertFalse(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));

        doReturn(mIncognitoProfileMock).when(mProfileSupplier).get();
        messageModel = mMediator.getNextMessageItemForType(MessageType.IPH).model;
        Assert.assertTrue(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));
    }

    @Test
    public void isMessageShownTest() {
        Assert.assertFalse(
                mMediator.isMessageShown(
                        MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME));
        enqueueMessageItem(MessageType.PRICE_MESSAGE, -1);
        // Mock pulling this message, which will move the message from mMessageItems to
        // mShownMessageItems.
        mMediator.getNextMessageItemForType(MessageType.PRICE_MESSAGE);
        Assert.assertTrue(
                mMediator.isMessageShown(
                        MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME));
    }
}
