// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
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
    private final Map<Integer, List<Message>> mMessageItems = new LinkedHashMap<>();
    private final Map<Integer, Message> mShownMessageItems = new LinkedHashMap<>();
    private final MessageCardView.ServiceDismissActionProvider mServiceDismissActionProvider;

    public MessageCardProviderMediator(
            Context context,
            Supplier<Profile> profileSupplier,
            MessageCardView.ServiceDismissActionProvider serviceDismissActionProvider) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mServiceDismissActionProvider = serviceDismissActionProvider;
    }

    /**
     * @return A list of {@link Message} that can be shown.
     */
    public List<Message> getMessageItems() {
        for (Iterator<Integer> it = mMessageItems.keySet().iterator(); it.hasNext(); ) {
            int key = it.next();
            if (mShownMessageItems.containsKey(key)) continue;

            List<Message> messages = mMessageItems.get(key);

            assumeNonNull(messages);
            assert !messages.isEmpty();
            mShownMessageItems.put(key, messages.remove(0));

            if (messages.isEmpty()) it.remove();
        }

        for (Message message : mShownMessageItems.values()) {
            PropertyModel model = message.model;
            if (!model.containsKey(MessageCardViewProperties.IS_INCOGNITO)) continue;
            model.set(
                    MessageCardViewProperties.IS_INCOGNITO,
                    mProfileSupplier.get().isOffTheRecord());
            model.set(TabListModel.CardProperties.CARD_ALPHA, 1F);
        }

        return new ArrayList<>(mShownMessageItems.values());
    }

    @Nullable Message getNextMessageItemForType(@MessageService.MessageType int messageType) {
        if (!mShownMessageItems.containsKey(messageType)) {
            if (!mMessageItems.containsKey(messageType)) return null;

            List<Message> messages = mMessageItems.get(messageType);

            assert !messages.isEmpty();
            mShownMessageItems.put(messageType, messages.remove(0));

            if (messages.isEmpty()) mMessageItems.remove(messageType);
        }

        Message message = mShownMessageItems.get(messageType);
        PropertyModel model = message.model;
        if (model.containsKey(MessageCardViewProperties.IS_INCOGNITO)) {
            model.set(
                    MessageCardViewProperties.IS_INCOGNITO,
                    mProfileSupplier.get().isOffTheRecord());
        }
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

    // MessageObserver implementations.

    @Override
    public void messageReady(
            @MessageService.MessageType int type, MessageService.MessageModelFactory factory) {
        assert !mShownMessageItems.containsKey(type);

        Message message = new Message(type, factory.build(mContext, this::invalidateShownMessage));
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
        mServiceDismissActionProvider.dismiss(type);
    }

    Map<Integer, List<Message>> getReadyMessageItemsForTesting() {
        return mMessageItems;
    }

    Map<Integer, Message> getShownMessageItemsForTesting() {
        return mShownMessageItems;
    }
}
