// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.Message;

import java.util.List;

/**
 * This is the coordinator for MessageCardProvider component. This component is used to build a
 * TabGridMessageCardView for each message type. This coordinator manages the life-cycle of all
 * shared components and the connection between all subscribed {@link MessageService}.
 *
 * @param <MessageT> The message type.
 * @param <UiT> The UI type.
 */
@NullMarked
public class MessageCardProviderCoordinator<MessageT, UiT> {
    private final MessageCardProviderMediator<MessageT, UiT> mMediator;
    private @Nullable MessageHostDelegate<MessageT, UiT> mMessageHostDelegate;

    MessageCardProviderCoordinator(
            Context context, ServiceDismissActionProvider<MessageT> serviceDismissActionProvider) {
        mMediator = new MessageCardProviderMediator<>(context, serviceDismissActionProvider);
    }

    /**
     * Binds a {@link MessageHostDelegate} to the coordinator and registers services to it.
     *
     * @param messageHostDelegate The {@link MessageHostDelegate} to bind.
     */
    public void bindHostDelegate(MessageHostDelegate<MessageT, UiT> messageHostDelegate) {
        mMessageHostDelegate = messageHostDelegate;

        for (MessageService<MessageT, UiT> service : mMediator.getMessageServices()) {
            mMessageHostDelegate.registerService(service);
        }
    }

    /**
     * Subscribes to a {@link MessageService} to get any message changes. @see MessageObserver.
     *
     * @param service The {@link MessageService} to subscribe.
     */
    public void subscribeMessageService(MessageService<MessageT, UiT> service) {
        if (mMessageHostDelegate != null) {
            mMessageHostDelegate.registerService(service);
        }

        // TODO(crbug.com/439557010): Simplify the observer interactions.
        // We must register the service to the mediator before registering the mediator to the
        // service.
        mMediator.addMessageService(service);
        service.addObserver(mMediator);
    }

    /**
     * Returns the next {@link Message} for the given messageType, if there is any. Otherwise
     * returns null.
     *
     * @param messageType The message type associates with the message.
     */
    public @Nullable Message<MessageT> getNextMessageItemForType(MessageT messageType) {
        return mMediator.getNextMessageItemForType(messageType);
    }

    /**
     * Whether the given message is shown.
     *
     * @param messageType The message type associated with the message.
     * @param identifier The identifier associated with the message.
     */
    boolean isMessageShown(MessageT messageType, int identifier) {
        return mMediator.isMessageShown(messageType, identifier);
    }

    /** Returns all registered message services. */
    public List<MessageService<MessageT, UiT>> getMessageServices() {
        return mMediator.getMessageServices();
    }

    /** Clean up all member fields. */
    public void destroy() {
        List<MessageService<MessageT, UiT>> services = mMediator.getMessageServices();
        for (int i = 0; i < services.size(); i++) {
            MessageService<MessageT, UiT> service = services.get(i);
            mMediator.removeMessageService(service);
            service.removeObserver(mMediator);
        }
    }
}
