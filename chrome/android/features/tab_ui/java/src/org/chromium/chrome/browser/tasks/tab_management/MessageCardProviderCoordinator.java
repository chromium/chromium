// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardProviderMediator.Message;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the coordinator for MessageCardProvider component. This component is used to build a
 * TabGridMessageCardView for each message type. This coordinator manages the life-cycle of all
 * shared components and the connection between all subscribed {@link MessageService}.
 *
 * @param <T> The message type.
 */
@NullMarked
public class MessageCardProviderCoordinator<T> {
    private final MessageCardProviderMediator<T> mMediator;
    private final List<MessageService<T>> mMessageServices = new ArrayList<>();

    MessageCardProviderCoordinator(
            Context context,
            Supplier<Profile> profileSupplier,
            ServiceDismissActionProvider<T> serviceDismissActionProvider) {
        mMediator =
                new MessageCardProviderMediator<>(
                        context, profileSupplier, serviceDismissActionProvider);
    }

    /**
     * Subscribes to a {@link MessageService} to get any message changes. @see MessageObserver.
     *
     * @param service The {@link MessageService} to subscribe.
     */
    public void subscribeMessageService(MessageService<T> service) {
        mMessageServices.add(service);
        service.addObserver(mMediator);
    }

    /**
     * Get all messages.
     *
     * @return a list of {@link Message}.
     */
    public List<Message<T>> getMessageItems() {
        return mMediator.getMessageItems();
    }

    /**
     * @param messageType The message type associates with the message.
     * @return The next {@link Message} for the given messageType, if there is any. Otherwise
     *     returns null.
     */
    public @Nullable Message<T> getNextMessageItemForType(T messageType) {
        return mMediator.getNextMessageItemForType(messageType);
    }

    /**
     * @param messageType The message type associated with the message.
     * @param identifier The identifier associated with the message.
     * @return Whether the given message is shown.
     */
    boolean isMessageShown(T messageType, int identifier) {
        return mMediator.isMessageShown(messageType, identifier);
    }

    /** Clean up all member fields. */
    public void destroy() {
        for (int i = 0; i < mMessageServices.size(); i++) {
            mMessageServices.get(i).removeObserver(mMediator);
        }
    }
}
