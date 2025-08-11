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
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageModelFactory;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * This is a {@link MessageObserver} that creates and owns different {@link PropertyModel} based on
 * the message type.
 *
 * @param <T> The message type.
 */
@NullMarked
public class MessageCardProviderMediator<T> implements MessageObserver<T> {
    /**
     * A class represents a Message.
     *
     * @param <T> The message type.
     */
    public static class Message<T> {
        public final T type;
        public final PropertyModel model;

        Message(T type, PropertyModel model) {
            this.type = type;
            this.model = model;
        }
    }

    private final Context mContext;
    private final Supplier<Profile> mProfileSupplier;
    private final Map<T, List<Message<T>>> mMessageItems = new LinkedHashMap<>();
    private final Map<T, Message<T>> mShownMessageItems = new LinkedHashMap<>();
    private final ServiceDismissActionProvider<T> mServiceDismissActionProvider;

    public MessageCardProviderMediator(
            Context context,
            Supplier<Profile> profileSupplier,
            ServiceDismissActionProvider<T> serviceDismissActionProvider) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mServiceDismissActionProvider = serviceDismissActionProvider;
    }

    /**
     * @return A list of {@link Message} that can be shown.
     */
    public List<Message<T>> getMessageItems() {
        for (Iterator<T> it = mMessageItems.keySet().iterator(); it.hasNext(); ) {
            T key = it.next();
            if (mShownMessageItems.containsKey(key)) continue;

            List<Message<T>> messages = mMessageItems.get(key);

            assumeNonNull(messages);
            assert !messages.isEmpty();
            mShownMessageItems.put(key, messages.remove(0));

            if (messages.isEmpty()) it.remove();
        }

        for (Message<T> message : mShownMessageItems.values()) {
            PropertyModel model = message.model;
            if (!model.containsKey(MessageCardViewProperties.IS_INCOGNITO)) continue;
            model.set(
                    MessageCardViewProperties.IS_INCOGNITO,
                    mProfileSupplier.get().isOffTheRecord());
            model.set(TabListModel.CardProperties.CARD_ALPHA, 1F);
        }

        return new ArrayList<>(mShownMessageItems.values());
    }

    @Nullable Message<T> getNextMessageItemForType(T messageType) {
        if (!mShownMessageItems.containsKey(messageType)) {
            if (!mMessageItems.containsKey(messageType)) return null;

            List<Message<T>> messages = mMessageItems.get(messageType);

            assert !messages.isEmpty();
            mShownMessageItems.put(messageType, messages.remove(0));

            if (messages.isEmpty()) mMessageItems.remove(messageType);
        }

        Message<T> message = mShownMessageItems.get(messageType);
        PropertyModel model = message.model;
        if (model.containsKey(MessageCardViewProperties.IS_INCOGNITO)) {
            model.set(
                    MessageCardViewProperties.IS_INCOGNITO,
                    mProfileSupplier.get().isOffTheRecord());
        }
        return message;
    }

    boolean isMessageShown(T messageType, int identifier) {
        if (!mShownMessageItems.containsKey(messageType)) return false;
        return mShownMessageItems
                        .get(messageType)
                        .model
                        .get(MessageCardViewProperties.MESSAGE_IDENTIFIER)
                == identifier;
    }

    // MessageObserver implementations.

    @Override
    public void messageReady(T type, MessageModelFactory<T> factory) {
        assert !mShownMessageItems.containsKey(type);

        Message<T> message =
                new Message<>(type, factory.build(mContext, this::invalidateShownMessage));
        if (mMessageItems.containsKey(type)) {
            mMessageItems.get(type).add(message);
        } else {
            mMessageItems.put(type, new ArrayList<>(Arrays.asList(message)));
        }
    }

    @Override
    public void messageInvalidate(T type) {
        if (mMessageItems.containsKey(type)) {
            mMessageItems.remove(type);
        }
        if (mShownMessageItems.containsKey(type)) {
            invalidateShownMessage(type);
        }
    }

    @VisibleForTesting
    void invalidateShownMessage(T type) {
        mShownMessageItems.remove(type);
        mServiceDismissActionProvider.dismiss(type);
    }

    Map<T, List<Message<T>>> getReadyMessageItemsForTesting() {
        return mMessageItems;
    }

    Map<T, Message<T>> getShownMessageItemsForTesting() {
        return mShownMessageItems;
    }
}
