// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.ARCHIVED_TABS_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.IPH;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.PRICE_MESSAGE;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * This is a {@link MessageService.MessageObserver} that creates and owns different {@link
 * PropertyModel} based on the message type.
 */
public class MessageCardProviderMediator implements MessageService.MessageObserver {
    /** A class represents a Message. */
    public static class Message {
        public final @MessageService.MessageType int type;
        public final PropertyModel model;

        Message(int type, PropertyModel model) {
            this.type = type;
            this.model = model;
        }
    }

    private final Context mContext;
    private final Supplier<Profile> mProfileSupplier;
    private Map<Integer, List<Message>> mMessageItems = new LinkedHashMap<>();
    private Map<Integer, Message> mShownMessageItems = new LinkedHashMap<>();
    private MessageCardView.DismissActionProvider mUiDismissActionProvider;

    public MessageCardProviderMediator(
            Context context,
            Supplier<Profile> profileSupplier,
            MessageCardView.DismissActionProvider uiDismissActionProvider) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mUiDismissActionProvider = uiDismissActionProvider;
    }

    /**
     * @return A list of {@link Message} that can be shown.
     */
    public List<Message> getMessageItems() {
        for (Iterator<Integer> it = mMessageItems.keySet().iterator(); it.hasNext(); ) {
            int key = it.next();
            if (mShownMessageItems.containsKey(key)) continue;

            List<Message> messages = mMessageItems.get(key);

            assert messages.size() > 0;
            mShownMessageItems.put(key, messages.remove(0));

            if (messages.size() == 0) it.remove();
        }

        for (Message message : mShownMessageItems.values()) {
            message.model.set(
                    MessageCardViewProperties.IS_INCOGNITO,
                    mProfileSupplier.get().isOffTheRecord());
            message.model.set(TabListModel.CardProperties.CARD_ALPHA, 1F);
        }

        return new ArrayList<>(mShownMessageItems.values());
    }

    Message getNextMessageItemForType(@MessageService.MessageType int messageType) {
        if (!mShownMessageItems.containsKey(messageType)) {
            if (!mMessageItems.containsKey(messageType)) return null;

            List<Message> messages = mMessageItems.get(messageType);

            assert messages.size() > 0;
            mShownMessageItems.put(messageType, messages.remove(0));

            if (messages.size() == 0) mMessageItems.remove(messageType);
        }

        Message message = mShownMessageItems.get(messageType);
        message.model.set(
                MessageCardViewProperties.IS_INCOGNITO, mProfileSupplier.get().isOffTheRecord());
        return message;
    }

    boolean isMessageShown(@MessageService.MessageType int messageType, int identifier) {
        if (!mShownMessageItems.containsKey(messageType)) return false;
        return mShownMessageItems
                        .get(messageType)
                        .model
                        .get(MessageCardViewProperties.MESSAGE_IDENTIFIER)
                == identifier;
    }

    private PropertyModel buildModel(int messageType, MessageService.MessageData data) {
        switch (messageType) {
            case IPH:
                assert data instanceof IphMessageService.IphMessageData;
                return IphMessageCardViewModel.create(
                        mContext,
                        this::invalidateShownMessage,
                        (IphMessageService.IphMessageData) data);
            case PRICE_MESSAGE:
                assert data instanceof PriceMessageService.PriceMessageData;
                return PriceMessageCardViewModel.create(
                        mContext,
                        this::invalidateShownMessage,
                        (PriceMessageService.PriceMessageData) data,
                        PriceDropNotificationManagerFactory.create(mProfileSupplier.get()));
            case INCOGNITO_REAUTH_PROMO_MESSAGE:
                assert data
                        instanceof IncognitoReauthPromoMessageService.IncognitoReauthMessageData;
                return IncognitoReauthPromoViewModel.create(
                        mContext,
                        this::invalidateShownMessage,
                        (IncognitoReauthPromoMessageService.IncognitoReauthMessageData) data);
            case ARCHIVED_TABS_MESSAGE:
                assert data instanceof ArchivedTabsMessageService.ArchivedTabsMessageData;
                return CustomMessageCardViewModel.create(
                        ((ArchivedTabsMessageService.ArchivedTabsMessageData) data).getProvider());
            default:
                return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MessageCardViewProperties.IS_INCOGNITO, false)
                        .build();
        }
    }

    // MessageObserver implementations.

    @Override
    public void messageReady(
            @MessageService.MessageType int type, MessageService.MessageData data) {
        assert !mShownMessageItems.containsKey(type);

        Message message = new Message(type, buildModel(type, data));
        if (mMessageItems.containsKey(type)) {
            mMessageItems.get(type).add(message);
        } else {
            mMessageItems.put(type, new ArrayList<>(Arrays.asList(message)));
        }
    }

    @Override
    public void messageInvalidate(@MessageService.MessageType int type) {
        if (mMessageItems.containsKey(type)) {
            mMessageItems.remove(type);
        }
        if (mShownMessageItems.containsKey(type)) {
            invalidateShownMessage(type);
        }
    }

    @VisibleForTesting
    void invalidateShownMessage(@MessageService.MessageType int type) {
        mShownMessageItems.remove(type);
        mUiDismissActionProvider.dismiss(type);
    }

    Map<Integer, List<Message>> getReadyMessageItemsForTesting() {
        return mMessageItems;
    }

    Map<Integer, Message> getShownMessageItemsForTesting() {
        return mShownMessageItems;
    }
}
