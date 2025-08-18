// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
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
 * @param <T> The message type.
 */
@NullMarked
public class MessageCardProviderMediator<T> implements MessageObserver<T> {
    private final Context mContext;
    private final Supplier<Profile> mProfileSupplier;
    private final ServiceDismissActionProvider<T> mServiceDismissActionProvider;
    private final Map<T, MessageService<T>> mMessageServices = new HashMap<>();

    public MessageCardProviderMediator(
            Context context,
            Supplier<Profile> profileSupplier,
            ServiceDismissActionProvider<T> serviceDismissActionProvider) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mServiceDismissActionProvider = serviceDismissActionProvider;
    }

    /**
     * Get the next message item for the given type.
     *
     * @param messageType The type of the message.
     * @return The next message item for the given type.
     */
    public @Nullable Message<T> getNextMessageItemForType(T messageType) {
        MessageService<T> service = mMessageServices.get(messageType);
        if (service == null) return null;

        Message<T> message = service.getNextMessageItem();
        if (message == null) return null;

        PropertyModel model = message.model;
        maybeSetCardIncognitoStatus(model);
        return message;
    }

    /**
     * @param messageType The message type associated with the message.
     * @param identifier The identifier associated with the message.
     * @return Whether the given message is shown.
     */
    boolean isMessageShown(T messageType, int identifier) {
        MessageService<T> service = mMessageServices.get(messageType);
        if (service == null) return false;
        return service.isMessageShown(identifier);
    }

    // MessageObserver implementations.
    @Override
    public void messageReady(T type, MessageModelFactory<T> factory) {
        MessageService<T> service = mMessageServices.get(type);
        if (service == null) return;

        PropertyModel model = factory.build(mContext, this::invalidateShownMessage);
        maybeSetCardIncognitoStatus(model);
        service.addMessage(new Message<>(type, model));
    }

    @Override
    public void messageInvalidate(T type) {
        MessageService<T> service = mMessageServices.get(type);
        if (service == null) return;

        service.invalidateMessages();
        invalidateShownMessage(type);
    }

    @VisibleForTesting
    void invalidateShownMessage(T type) {
        MessageService<T> service = mMessageServices.get(type);
        if (service == null) return;

        service.invalidateShownMessage();
        mServiceDismissActionProvider.dismiss(type);
    }

    /**
     * Add a message service to the mediator.
     *
     * @param service The message service to add.
     */
    public void addMessageService(MessageService<T> service) {
        mMessageServices.put(service.getMessageType(), service);
    }

    /**
     * Remove a message service from the mediator.
     *
     * @param service The message service to remove.
     */
    public void removeMessageService(MessageService<T> service) {
        mMessageServices.remove(service.getMessageType());
    }

    /** Returns a list of the registered message services. */
    public List<MessageService<T>> getMessageServices() {
        return new ArrayList<>(mMessageServices.values());
    }

    @VisibleForTesting
    Map<T, MessageService<T>> getMessageServicesMap() {
        return mMessageServices;
    }

    private void maybeSetCardIncognitoStatus(PropertyModel model) {
        if (model.containsKey(MessageCardViewProperties.IS_INCOGNITO)) {
            model.set(
                    MessageCardViewProperties.IS_INCOGNITO,
                    mProfileSupplier.get().isOffTheRecord());
        }
    }
}
