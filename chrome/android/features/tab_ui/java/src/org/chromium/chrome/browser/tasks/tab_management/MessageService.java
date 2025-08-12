// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.CallSuper;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Ideally, each message type, <T>, requires a MessageService class. This is the base class. All the
 * concrete subclass should contain logic that convert the data from the corresponding external
 * service to a data structure that the TabGridMessageCardProvider understands.
 *
 * @param <T> The message type.
 */
@NullMarked
public class MessageService<T> {

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

    MessageService(T messageType) {
        mMessageType = messageType;
    }

    @CallSuper
    public void destroy() {
        mObservers.clear();
    }

    /** Add a {@link MessageObserver} to be notified when message from external service changes. */
    public void addObserver(MessageObserver<T> observer) {
        mObservers.addObserver(observer);
    }

    /** Remove a {@link MessageObserver}. */
    public void removeObserver(MessageObserver<T> observer) {
        mObservers.removeObserver(observer);
    }

    protected ObserverList<MessageObserver<T>> getObserversForTesting() {
        return mObservers;
    }

    /** Notifies all {@link MessageObserver} that a message is available. */
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
}
