// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the coordinator for MessageCardProvider component. This component is used to build a
 * TabGridMessageCardView for each {@link MessageService.MessageType}. This coordinator manages the
 * life-cycle of all shared components and the connection between all subscribed
 * {@link MessageService}.
 */
public class MessageCardProviderCoordinator {
    private final MessageCardProviderMediator mMediator;
    private final List<MessageService> mMessageServices = new ArrayList<>();

    MessageCardProviderCoordinator(
            Context context,
            Supplier<Profile> profileSupplier,
            MessageCardView.DismissActionProvider uiDismissActionProvider) {
        mMediator =
                new MessageCardProviderMediator(context, profileSupplier, uiDismissActionProvider);
    }

    /**
     * Subscribes to a {@link MessageService} to get any message changes. @see MessageObserver.
     *
     * @param service The {@link MessageService} to subscribe.
     */
    public void subscribeMessageService(MessageService service) {
        mMessageServices.add(service);
        service.addObserver(mMediator);
    }

    /**
     * Get all messages.
     * @return a list of {@link MessageCardProviderMediator.Message}.
     */
    public List<MessageCardProviderMediator.Message> getMessageItems() {
        return mMediator.getMessageItems();
    }

    /**
     * @param messageType The {@link MessageService#mMessageType} associates with the message.
     * @return The next {@link MessageCardProviderMediator.Message} for the given messageType, if
     *         there is any. Otherwise returns null.
     */
    @Nullable
    public MessageCardProviderMediator.Message getNextMessageItemForType(
            @MessageService.MessageType int messageType) {
        return mMediator.getNextMessageItemForType(messageType);
    }

    /**
     * @param messageType The {@link MessageService.MessageType} associated with the message.
     * @param identifier The identifier associated with the message.
     * @return Whether the given message is shown.
     */
    boolean isMessageShown(@MessageService.MessageType int messageType, int identifier) {
        return mMediator.isMessageShown(messageType, identifier);
    }

    /** Clean up all member fields. */
    public void destroy() {
        for (int i = 0; i < mMessageServices.size(); i++) {
            mMessageServices.get(i).removeObserver(mMediator);
        }
    }
}
