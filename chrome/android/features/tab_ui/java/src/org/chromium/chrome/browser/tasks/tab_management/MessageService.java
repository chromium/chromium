// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
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
         * @param msgServiceDismissRunnable To be called when the message is dismissed to inform the
         *     message service.
         */
        PropertyModel build(ServiceDismissActionProvider<MessageT> msgServiceDismissRunnable);
    }

    private final MessageT mMessageType;
    private final UiT mUiType;
    private final @LayoutRes int mLayoutRes;
    private final ViewBinder<PropertyModel, ? extends View, PropertyKey> mBinder;
    private final Deque<Message<MessageT>> mMessageItems = new ArrayDeque<>();
    private @Nullable Message<MessageT> mShownMessage;
    @MonotonicNonNull private ServiceDismissActionProvider<MessageT> mServiceDismissActionProvider;

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

    /**
     * Initializes the service and sets the service dismiss runnable.
     *
     * @param serviceDismissActionProvider Performs a cleanup operations to remove a given message
     *     from the UI.
     */
    @Initializer
    @CallSuper
    public void initialize(ServiceDismissActionProvider<MessageT> serviceDismissActionProvider) {
        assert mServiceDismissActionProvider == null;
        mServiceDismissActionProvider = serviceDismissActionProvider;
    }

    /**
     * Queues a message item, allowing it to be shown at the next {@link #getNextMessageItem()}
     * call.
     */
    public void queueMessage(MessageModelFactory<MessageT> data) {
        assert mServiceDismissActionProvider != null;

        PropertyModel model = data.build(mServiceDismissActionProvider);
        mMessageItems.add(new Message<>(mMessageType, model));
    }

    /**
     * Invalidate all messages, including the one currently shown. This will remove all messages
     * from the queue.
     */
    public void invalidateMessages() {
        mMessageItems.clear();
        dismissShownMessage();
    }

    /**
     * Invalidates the currently shown message and removes it from the UI. Used when it is not
     * required to clear the entire message queue.
     */
    public void dismissShownMessage() {
        assert mServiceDismissActionProvider != null;

        mShownMessage = null;
        mServiceDismissActionProvider.dismiss(mMessageType);
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
