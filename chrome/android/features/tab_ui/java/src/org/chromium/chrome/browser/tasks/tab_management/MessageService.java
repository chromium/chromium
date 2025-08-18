// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

/**
 * Ideally, each message type, <T>, requires a MessageService class. This is the base class. All the
 * concrete subclass should contain logic that convert the data from the corresponding external
 * service to a data structure that the TabGridMessageCardProvider understands.
 *
 * @param <T> The message type.
 */
@NullMarked
public class MessageService<T> {
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

    public static final int DEFAULT_MESSAGE_IDENTIFIER = -1;

    /**
     * Used to build the property model for a message.
     *
     * @param <T> The message type.
     */
    @FunctionalInterface
    public interface MessageModelFactory<T> {
        /**
         * Builds the property model for the message.
         *
         * @param context The context for the message.
         * @param msgServiceDismissRunnable To be called when the message is dismissed to inform the
         *     message service.
         */
        PropertyModel build(
                Context context, ServiceDismissActionProvider<T> msgServiceDismissRunnable);
    }

    /**
     * An interface to be notified about changes to a Message.
     *
     * @param <T> The message type.
     */
    public interface MessageObserver<T> {
        /**
         * Called when a message is available.
         *
         * @param type The type of the message.
         * @param data {@link MessageModelFactory} associated with the message.
         */
        void messageReady(T type, MessageModelFactory<T> data);

        /**
         * Called when a message is invalidated.
         *
         * @param type The type of the message.
         */
        void messageInvalidate(T type);
    }

    private final ObserverList<MessageObserver<T>> mObservers = new ObserverList<>();
    private final T mMessageType;
    private final Deque<Message<T>> mMessageItems = new ArrayDeque<>();
    private @Nullable Message<T> mShownMessage;

    MessageService(T messageType) {
        mMessageType = messageType;
    }

    @CallSuper
    public void destroy() {
        mObservers.clear();
    }

    /**
     * Add a {@link MessageObserver} to be notified when message from external service changes.
     *
     * @param observer The observer to add.
     */
    public void addObserver(MessageObserver<T> observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove a {@link MessageObserver}.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(MessageObserver<T> observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies all {@link MessageObserver} that a message is available.
     *
     * @param data The factory to build the message model.
     */
    public void sendAvailabilityNotification(MessageModelFactory<T> data) {
        for (MessageObserver<T> observer : mObservers) {
            observer.messageReady(mMessageType, data);
        }
    }

    /** Notifies all {@link MessageObserver} that a message was invalidated. */
    public void sendInvalidNotification() {
        for (MessageObserver<T> observer : mObservers) {
            observer.messageInvalidate(mMessageType);
        }
    }

    /**
     * Add a message to the message list.
     *
     * @param message The message to add.
     */
    public void addMessage(Message<T> message) {
        mMessageItems.add(message);
    }

    /**
     * Invalidate all messages, including the one currently shown. This will remove all messages
     * from the queue.
     */
    public void invalidateMessages() {
        mMessageItems.clear();
        mShownMessage = null;
    }

    /**
     * Returns the next {@link Message} to be shown, if there is any. If a message is already shown,
     * it will be returned. If not, the next message in the queue will be returned and set as shown.
     */
    public @Nullable Message<T> getNextMessageItem() {
        if (mShownMessage == null && !mMessageItems.isEmpty()) {
            mShownMessage = mMessageItems.removeFirst();
        }
        return mShownMessage;
    }

    /**
     * Checks if the message with the given identifier is currently shown.
     *
     * @param identifier The identifier of the message.
     */
    public boolean isMessageShown(int identifier) {
        if (mShownMessage == null) return false;
        return mShownMessage.model.get(MessageCardViewProperties.MESSAGE_IDENTIFIER) == identifier;
    }

    /**
     * Invalidate the currently shown message. The next message in the queue will be shown on the
     * next call to {@link #getNextMessageItem()}.
     */
    public void invalidateShownMessage() {
        mShownMessage = null;
    }

    protected ObserverList<MessageObserver<T>> getObserversForTesting() {
        return mObservers;
    }

    @VisibleForTesting
    List<Message<T>> getMessageItems() {
        return new ArrayList<>(mMessageItems);
    }

    @Nullable
    @VisibleForTesting
    Message<T> getShownMessage() {
        return mShownMessage;
    }

    /** Returns the message type of this service. */
    public T getMessageType() {
        return mMessageType;
    }
}
