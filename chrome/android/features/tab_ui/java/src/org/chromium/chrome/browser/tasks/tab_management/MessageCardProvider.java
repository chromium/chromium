// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.Message;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageModelFactory;
import org.chromium.ui.modelutil.PropertyModel;

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
    private final Context mContext;
    private final ServiceDismissActionProvider<MessageT> mServiceDismissActionProvider;
    private final Map<MessageT, MessageService<MessageT, UiT>> mMessageServices = new HashMap<>();
    private final MessageService.MessageObserver<MessageT> mObserver =
            new MessageService.MessageObserver<>() {
                @Override
                public void messageReady(MessageT type, MessageModelFactory<MessageT> factory) {
                    MessageService<MessageT, UiT> service = mMessageServices.get(type);
                    if (service == null) return;

                    PropertyModel model =
                            factory.build(
                                    mContext, MessageCardProvider.this::invalidateShownMessage);
                    service.addMessage(new Message<>(type, model));
                }

                @Override
                public void messageInvalidate(MessageT type) {
                    MessageService<MessageT, UiT> service = mMessageServices.get(type);
                    if (service == null) return;

                    service.invalidateMessages();
                    invalidateShownMessage(type);
                }
            };

    private @Nullable MessageHostDelegate<MessageT, UiT> mMessageHostDelegate;

    MessageCardProvider(
            Context context, ServiceDismissActionProvider<MessageT> serviceDismissActionProvider) {
        mContext = context;
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

        // TODO(crbug.com/439557010): Simplify the observer interactions.
        mMessageServices.put(service.getMessageType(), service);
        service.addObserver(mObserver);
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
        List<MessageService<MessageT, UiT>> services = getMessageServices();
        for (int i = 0; i < services.size(); i++) {
            MessageService<MessageT, UiT> service = services.get(i);
            mMessageServices.remove(service.getMessageType());
            service.removeObserver(mObserver);
        }
    }

    @VisibleForTesting
    void invalidateShownMessage(MessageT type) {
        MessageService<MessageT, UiT> service = mMessageServices.get(type);
        if (service == null) return;

        service.invalidateShownMessage();
        mServiceDismissActionProvider.dismiss(type);
    }

    @VisibleForTesting
    Map<MessageT, MessageService<MessageT, UiT>> getMessageServicesMap() {
        return mMessageServices;
    }
}
