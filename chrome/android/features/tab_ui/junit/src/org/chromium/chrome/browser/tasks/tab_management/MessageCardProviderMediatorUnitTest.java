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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link MessageCardProviderMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageCardProviderMediatorUnitTest {
    private static final int SUGGESTED_TAB_COUNT = 2;
    private static final int TESTING_ACTION = -1;

    private MessageCardProviderMediator mMediator;

    @Mock private MessageCardView.DismissActionProvider mUiDismissActionProvider;

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
        MockitoAnnotations.initMocks(this);

        doReturn(true).when(mIncognitoProfileMock).isOffTheRecord();
        doReturn(mProfileMock).when(mProfileSupplier).get();
        doNothing().when(mUiDismissActionProvider).dismiss(anyInt());
        mMediator =
                new MessageCardProviderMediator(
                        mContext, mProfileSupplier, mUiDismissActionProvider);
    }

    private void enqueueMessageItem(@MessageService.MessageType int type, int tabSuggestionAction) {
        switch (type) {
            case MessageService.MessageType.PRICE_MESSAGE:
                when(mPriceMessageData.getPriceDrop()).thenReturn(null);
                when(mPriceMessageData.getDismissActionProvider()).thenReturn((messageType) -> {});
                when(mPriceMessageData.getReviewActionProvider()).thenReturn(() -> {});
                when(mPriceMessageData.getType()).thenReturn(PriceMessageType.PRICE_WELCOME);
                mMediator.messageReady(type, mPriceMessageData);
                break;
            case MessageService.MessageType.IPH:
                when(mIphMessageData.getDismissActionProvider()).thenReturn((messageType) -> {});
                when(mIphMessageData.getReviewActionProvider()).thenReturn(() -> {});
                mMediator.messageReady(type, mIphMessageData);
                break;
            case MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE:
                when(mIncognitoReauthMessageData.getReviewActionProvider()).thenReturn(() -> {});
                mMediator.messageReady(type, mIncognitoReauthMessageData);
                break;
            default:
                mMediator.messageReady(type, new MessageService.MessageData() {});
        }
    }

    @Test
    public void getMessageItemsTest() {
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void getMessageItemsTest_TwoDifferentTypeMessage() {
        enqueueMessageItem(MessageService.MessageType.PRICE_MESSAGE, -1);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());

        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        Assert.assertEquals(2, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void getMessageItemsTest_OneMessageForEachMessageType() {
        enqueueMessageItem(MessageService.MessageType.PRICE_MESSAGE, -1);
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        List<MessageCardProviderMediator.Message> messages = mMediator.getMessageItems();
        Assert.assertEquals(2, messages.size());
        Assert.assertEquals(MessageService.MessageType.PRICE_MESSAGE, messages.get(0).type);
        Assert.assertEquals(MessageService.MessageType.FOR_TESTING, messages.get(1).type);

        Assert.assertEquals(2, mMediator.getShownMessageItemsForTesting().size());
        Assert.assertTrue(
                mMediator
                        .getShownMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.PRICE_MESSAGE));
        Assert.assertTrue(
                mMediator
                        .getShownMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.FOR_TESTING));
    }

    @Test
    public void getMessageItemsTest_ReturnFirstMessageFromMultipleSameTypeMessages() {
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        List<MessageCardProviderMediator.Message> messages =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.FOR_TESTING);
        Assert.assertEquals(2, messages.size());
        final MessageCardProviderMediator.Message testingMessage1 = messages.get(0);
        final MessageCardProviderMediator.Message testingMessage2 = messages.get(1);

        messages = mMediator.getMessageItems();
        Assert.assertEquals(1, messages.size());
        Assert.assertEquals(testingMessage1, messages.get(0));

        Assert.assertEquals(1, mMediator.getShownMessageItemsForTesting().size());
        Assert.assertEquals(
                testingMessage1,
                mMediator
                        .getShownMessageItemsForTesting()
                        .get(MessageService.MessageType.FOR_TESTING));

        Assert.assertEquals(1, mMediator.getShownMessageItemsForTesting().size());
        Assert.assertEquals(
                testingMessage2,
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.FOR_TESTING)
                        .get(0));
    }

    @Test
    public void getMessageItemsTest_PersistUntilInvalidationOccurred() {
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        List<MessageCardProviderMediator.Message> messages =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.FOR_TESTING);
        Assert.assertEquals(2, messages.size());
        final MessageCardProviderMediator.Message testingMessage1 = messages.get(0);

        // Test message is persisted.
        for (int i = 0; i < 2; i++) {
            messages = mMediator.getMessageItems();
            Assert.assertEquals(1, messages.size());
            Assert.assertEquals(testingMessage1, messages.get(0));
        }

        // Test message updated after invalidation, and the updated message is persisted.
        mMediator.invalidateShownMessage(MessageService.MessageType.FOR_TESTING);
        messages = mMediator.getMessageItems();
        final MessageCardProviderMediator.Message newMessage = messages.get(0);
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
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        List<MessageCardProviderMediator.Message> messages =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.FOR_TESTING);
        Assert.assertEquals(2, messages.size());
        final MessageCardProviderMediator.Message testingMessage1 = messages.get(0);
        final MessageCardProviderMediator.Message testingMessage2 = messages.get(1);

        messages = mMediator.getMessageItems();
        Assert.assertEquals(1, messages.size());
        Assert.assertEquals(testingMessage1, messages.get(0));

        mMediator.invalidateShownMessage(MessageService.MessageType.FOR_TESTING);

        messages = mMediator.getMessageItems();
        Assert.assertEquals(1, messages.size());
        Assert.assertEquals(testingMessage2, messages.get(0));

        mMediator.invalidateShownMessage(MessageService.MessageType.FOR_TESTING);
        Assert.assertEquals(0, mMediator.getMessageItems().size());
    }

    @Test
    public void invalidate_allMessages() {
        enqueueMessageItem(MessageService.MessageType.PRICE_MESSAGE, -1);

        mMediator.messageInvalidate(MessageService.MessageType.PRICE_MESSAGE);

        Assert.assertFalse(
                mMediator
                        .getReadyMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.PRICE_MESSAGE));
        Assert.assertFalse(
                mMediator
                        .getShownMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.PRICE_MESSAGE));

        // Testing multiple Messages has the same type.
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        mMediator.messageInvalidate(MessageService.MessageType.FOR_TESTING);
        Assert.assertFalse(
                mMediator
                        .getReadyMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.FOR_TESTING));
        Assert.assertFalse(
                mMediator
                        .getShownMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.FOR_TESTING));
    }

    @Test
    public void invalidate_shownMessage() {
        enqueueMessageItem(MessageService.MessageType.PRICE_MESSAGE, -1);

        mMediator.getMessageItems();
        mMediator.invalidateShownMessage(MessageService.MessageType.PRICE_MESSAGE);

        verify(mUiDismissActionProvider).dismiss(anyInt());
        Assert.assertFalse(
                mMediator
                        .getShownMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.PRICE_MESSAGE));
        Assert.assertFalse(
                mMediator
                        .getReadyMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.PRICE_MESSAGE));

        // Testing multiple Messages has the same type.
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);
        enqueueMessageItem(MessageService.MessageType.FOR_TESTING, TESTING_ACTION);

        mMediator.getMessageItems();
        mMediator.invalidateShownMessage(MessageService.MessageType.FOR_TESTING);
        Assert.assertFalse(
                mMediator
                        .getShownMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.FOR_TESTING));
        Assert.assertTrue(
                mMediator
                        .getReadyMessageItemsForTesting()
                        .containsKey(MessageService.MessageType.FOR_TESTING));
    }

    @Test
    public void buildModel_ForPriceMessage() {
        String titleText = "Price drop spotted";
        doReturn(titleText).when(mContext).getString(R.string.price_drop_spotted_title);

        enqueueMessageItem(MessageService.MessageType.PRICE_MESSAGE, -1);

        PropertyModel model =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.PRICE_MESSAGE)
                        .get(0)
                        .model;
        Assert.assertEquals(
                MessageService.MessageType.PRICE_MESSAGE,
                model.get(MessageCardViewProperties.MESSAGE_TYPE));
        Assert.assertEquals(titleText, model.get(MessageCardViewProperties.TITLE_TEXT));
    }

    @Test
    public void buildModel_ForIphMessage() {
        enqueueMessageItem(MessageService.MessageType.IPH, -1);

        PropertyModel model =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.IPH)
                        .get(0)
                        .model;
        Assert.assertEquals(
                MessageService.MessageType.IPH, model.get(MessageCardViewProperties.MESSAGE_TYPE));
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

        enqueueMessageItem(MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE, -1);

        PropertyModel model =
                mMediator
                        .getReadyMessageItemsForTesting()
                        .get(MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE)
                        .get(0)
                        .model;
        Assert.assertEquals(
                MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                model.get(MessageCardViewProperties.MESSAGE_TYPE));
        verify(mResourcesMock, times(1))
                .getDimensionPixelSize(R.dimen.incognito_reauth_promo_message_icon_height);
        verify(mResourcesMock, times(1))
                .getDimensionPixelSize(R.dimen.incognito_reauth_promo_message_icon_width);
        verify(mContext, times(2)).getResources();
    }

    @Test
    public void getMessageItemsTest_UpdateIncognito() {
        enqueueMessageItem(MessageService.MessageType.IPH, -1);

        PropertyModel messageModel = mMediator.getMessageItems().get(0).model;
        Assert.assertFalse(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));

        doReturn(mIncognitoProfileMock).when(mProfileSupplier).get();
        messageModel = mMediator.getMessageItems().get(0).model;
        Assert.assertTrue(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));
    }

    @Test
    public void getNextMessageItemForTypeTest_UpdateIncognito() {
        enqueueMessageItem(MessageService.MessageType.IPH, -1);

        PropertyModel messageModel =
                mMediator.getNextMessageItemForType(MessageService.MessageType.IPH).model;
        Assert.assertFalse(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));

        doReturn(mIncognitoProfileMock).when(mProfileSupplier).get();
        messageModel = mMediator.getNextMessageItemForType(MessageService.MessageType.IPH).model;
        Assert.assertTrue(messageModel.get(MessageCardViewProperties.IS_INCOGNITO));
    }

    @Test
    public void isMessageShownTest() {
        Assert.assertFalse(
                mMediator.isMessageShown(
                        MessageService.MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME));
        enqueueMessageItem(MessageService.MessageType.PRICE_MESSAGE, -1);
        // Mock pulling this message, which will move the message from mMessageItems to
        // mShownMessageItems.
        mMediator.getNextMessageItemForType(MessageService.MessageType.PRICE_MESSAGE);
        Assert.assertTrue(
                mMediator.isMessageShown(
                        MessageService.MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME));
    }
}
