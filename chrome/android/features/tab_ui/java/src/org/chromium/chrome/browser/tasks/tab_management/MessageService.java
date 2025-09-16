// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

/**
 * Ideally, each message type, <MessageT>, requires a MessageService class. This is the base class.
 * All the concrete subclass should contain logic that convert the data from the corresponding
 * external service to a data structure that the TabGridMessageCardProvider understands.
 *
 * @param <MessageT> The message type.
 * @param <UiT> The UI type.
 */
@NullMarked
public class MessageService<MessageT, UiT> {
    /**
     * A class represents a Message.
     *
     * @param <MessageType> The message type.
     */
    public static class Message<MessageType> {
        public final MessageType type;
        public final PropertyModel model;

        Message(MessageType type, PropertyModel model) {
            this.type = type;
            this.model = model;
        }
    }

    public static final int DEFAULT_MESSAGE_IDENTIFIER = -1;

    /**
     * Used to build the property model for a message.
     *
     * @param <MessageT> The message type.
     */
    @FunctionalInterface
    public interface MessageModelFactory<MessageT> {
        /**
         * Builds the property model for the message.
         *
         * @param context The context for the message.
         * @param msgServiceDismissRunnable To be called when the message is dismissed to inform the
         *     message service.
         */
        PropertyModel build(
                Context context, ServiceDismissActionProvider<MessageT> msgServiceDismissRunnable);
    }

    /**
     * An interface to be notified about changes to a Message.
     *
     * @param <MessageT> The message type.
     */
    public interface MessageObserver<MessageT> {
        /**
         * Called when a message is available.
         *
         * @param type The type of the message.
         * @param data {@link MessageModelFactory} associated with the message.
         */
        void messageReady(MessageT type, MessageModelFactory<MessageT> data);

        /**
         * Called when a message is invalidated.
         *
         * @param type The type of the message.
         */
        void messageInvalidate(MessageT type);
    }

    private final ObserverList<MessageObserver<MessageT>> mObservers = new ObserverList<>();
    private final MessageT mMessageType;
    private final UiT mUiType;
    private final @LayoutRes int mLayoutRes;
    private final ViewBinder<PropertyModel, ? extends View, PropertyKey> mBinder;
    private final Deque<Message<MessageT>> mMessageItems = new ArrayDeque<>();
    private @Nullable Message<MessageT> mShownMessage;

    MessageService(
            MessageT messageType,
            UiT uiType,
            @LayoutRes int layoutRes,
            ViewBinder<PropertyModel, ? extends View, PropertyKey> binder) {
        mMessageType = messageType;
        mUiType = uiType;
        mLayoutRes = layoutRes;
        mBinder = binder;
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
    public void addObserver(MessageObserver<MessageT> observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove a {@link MessageObserver}.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(MessageObserver<MessageT> observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies all {@link MessageObserver} that a message is available.
     *
     * @param data The factory to build the message model.
     */
    public void sendAvailabilityNotification(MessageModelFactory<MessageT> data) {
        for (MessageObserver<MessageT> observer : mObservers) {
            observer.messageReady(mMessageType, data);
        }
    }

    /** Notifies all {@link MessageObserver} that a message was invalidated. */
    public void sendInvalidNotification() {
        for (MessageObserver<MessageT> observer : mObservers) {
            observer.messageInvalidate(mMessageType);
        }
    }

    /**
     * Add a message to the message list.
     *
     * @param message The message to add.
     */
    public void addMessage(Message<MessageT> message) {
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
    public @Nullable Message<MessageT> getNextMessageItem() {
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

    protected ObserverList<MessageObserver<MessageT>> getObserversForTesting() {
        return mObservers;
    }

    @VisibleForTesting
    List<Message<MessageT>> getMessageItems() {
        return new ArrayList<>(mMessageItems);
    }

    @Nullable
    @VisibleForTesting
    Message<MessageT> getShownMessage() {
        return mShownMessage;
    }

    /** Returns the message type of this service. */
    public MessageT getMessageType() {
        return mMessageType;
    }

    /** Returns the UI type of the messages created by this service. */
    public UiT getUiType() {
        return mUiType;
    }

    /** Returns the layout resource for the message's UI. */
    public @LayoutRes int getLayout() {
        return mLayoutRes;
    }

    /** Returns the {@link ViewBinder} for the message's UI. */
    public ViewBinder<PropertyModel, ? extends View, PropertyKey> getBinder() {
        return mBinder;
    }
}
