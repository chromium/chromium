// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.Message;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This provides messages and manages the life-cycle of all shared components and subscribed {@link
 * MessageService}s.
 *
 * @param <MessageT> The message type.
 * @param <UiT> The UI type.
 */
@NullMarked
public class MessageCardProvider<MessageT, UiT> {
    private final ServiceDismissActionProvider<MessageT> mServiceDismissActionProvider;
    private final Map<MessageT, MessageService<MessageT, UiT>> mMessageServices = new HashMap<>();

    private @Nullable MessageHostDelegate<MessageT, UiT> mMessageHostDelegate;

    MessageCardProvider(ServiceDismissActionProvider<MessageT> serviceDismissActionProvider) {
        mServiceDismissActionProvider = serviceDismissActionProvider;
    }

    /**
     * Binds a {@link MessageHostDelegate} to the coordinator and registers services to it.
     *
     * @param messageHostDelegate The {@link MessageHostDelegate} to bind.
     */
    public void bindHostDelegate(MessageHostDelegate<MessageT, UiT> messageHostDelegate) {
        mMessageHostDelegate = messageHostDelegate;

        for (MessageService<MessageT, UiT> service : mMessageServices.values()) {
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

        mMessageServices.put(service.getMessageType(), service);
        service.initialize(mServiceDismissActionProvider);
    }

    /**
     * Returns the next {@link Message} for the given messageType, if there is any. Otherwise
     * returns null.
     *
     * @param messageType The message type associates with the message.
     */
    public @Nullable Message<MessageT> getNextMessageItemForType(MessageT messageType) {
        MessageService<MessageT, UiT> service = mMessageServices.get(messageType);
        if (service == null) return null;
        return service.getNextMessageItem();
    }

    /**
     * Whether the given message is shown.
     *
     * @param messageType The message type associated with the message.
     * @param identifier The identifier associated with the message.
     */
    boolean isMessageShown(MessageT messageType, int identifier) {
        MessageService<MessageT, UiT> service = mMessageServices.get(messageType);
        if (service == null) return false;
        return service.isMessageShown(identifier);
    }

    /** Returns all registered message services. */
    public List<MessageService<MessageT, UiT>> getMessageServices() {
        return new ArrayList<>(mMessageServices.values());
    }

    /** Clean up all member fields. */
    public void destroy() {
        mMessageServices.clear();
    }

    @VisibleForTesting
    Map<MessageT, MessageService<MessageT, UiT>> getMessageServicesMap() {
        return mMessageServices;
    }
}
