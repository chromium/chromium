// Copyright 2019 The Chromium Authors
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
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This is a {@link MessageObserver} that creates and owns different {@link PropertyModel} based on
 * the message type.
 *
 * @param <MessageT> The message type.
 * @param <UiT> The UI type.
 */
@NullMarked
public class MessageCardProviderMediator<MessageT, UiT> implements MessageObserver<MessageT> {
    private final Context mContext;
    private final ServiceDismissActionProvider<MessageT> mServiceDismissActionProvider;
    private final Map<MessageT, MessageService<MessageT, UiT>> mMessageServices = new HashMap<>();

    public MessageCardProviderMediator(
            Context context, ServiceDismissActionProvider<MessageT> serviceDismissActionProvider) {
        mContext = context;
        mServiceDismissActionProvider = serviceDismissActionProvider;
    }

    /**
     * Get the next message item for the given type.
     *
     * @param messageType The type of the message.
     * @return The next message item for the given type.
     */
    public @Nullable Message<MessageT> getNextMessageItemForType(MessageT messageType) {
        MessageService<MessageT, UiT> service = mMessageServices.get(messageType);
        if (service == null) return null;
        return service.getNextMessageItem();
    }

    /**
     * @param messageType The message type associated with the message.
     * @param identifier The identifier associated with the message.
     * @return Whether the given message is shown.
     */
    boolean isMessageShown(MessageT messageType, int identifier) {
        MessageService<MessageT, UiT> service = mMessageServices.get(messageType);
        if (service == null) return false;
        return service.isMessageShown(identifier);
    }

    // MessageObserver implementations.
    @Override
    public void messageReady(MessageT type, MessageModelFactory<MessageT> factory) {
        MessageService<MessageT, UiT> service = mMessageServices.get(type);
        if (service == null) return;

        PropertyModel model = factory.build(mContext, this::invalidateShownMessage);
        service.addMessage(new Message<>(type, model));
    }

    @Override
    public void messageInvalidate(MessageT type) {
        MessageService<MessageT, UiT> service = mMessageServices.get(type);
        if (service == null) return;

        service.invalidateMessages();
        invalidateShownMessage(type);
    }

    @VisibleForTesting
    void invalidateShownMessage(MessageT type) {
        MessageService<MessageT, UiT> service = mMessageServices.get(type);
        if (service == null) return;

        service.invalidateShownMessage();
        mServiceDismissActionProvider.dismiss(type);
    }

    /**
     * Add a message service to the mediator.
     *
     * @param service The message service to add.
     */
    public void addMessageService(MessageService<MessageT, UiT> service) {
        mMessageServices.put(service.getMessageType(), service);
    }

    /**
     * Remove a message service from the mediator.
     *
     * @param service The message service to remove.
     */
    public void removeMessageService(MessageService<MessageT, UiT> service) {
        mMessageServices.remove(service.getMessageType());
    }

    /** Returns a list of the registered message services. */
    public List<MessageService<MessageT, UiT>> getMessageServices() {
        return new ArrayList<>(mMessageServices.values());
    }

    @VisibleForTesting
    Map<MessageT, MessageService<MessageT, UiT>> getMessageServicesMap() {
        return mMessageServices;
    }
}
