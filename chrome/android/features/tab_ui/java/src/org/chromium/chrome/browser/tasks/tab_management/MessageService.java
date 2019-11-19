// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Ideally, for each of the {@link MessageType} requires a MessageService class. This is the
 * base class. All the concrete subclass should contain logic that convert the data from the
 * corresponding external service to a data structure that the TabGridMessageCardProvider
 * understands.
 */
public class MessageService {
    @IntDef({MessageType.TAB_SUGGESTION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageType {
        int FOR_TESTING = 0;
        int TAB_SUGGESTION = 1;
    }

    /**
     * This is a data wrapper. Implement this interface to send notification with data to all the
     * observers.
     *
     * @see #sendAvailabilityNotification(MessageData).
     */
    public interface MessageData {}

    /**
     * An interface to be notified about changes to a Message.
     * TODO(meiliang): Need to define this interface in more detail.
     */
    public interface MessageObserver {
        /**
         * Called when a message is available.
         * TODO(meiliang): message data is needed.
         * @param type The type of the message.
         * @param data {@link MessageData} associated with the message.
         */
        void messageReady(@MessageType int type, MessageData data);

        /**
         * Called when a message is invalidated.
         * @param type The type of the message.
         */
        void messageInvalidate(@MessageType int type);
    }

    ObserverList<MessageObserver> mObservers = new ObserverList<>();
    @MessageType
    int mMessageType;

    MessageService(@MessageType int mMessageType) {
        this.mMessageType = mMessageType;
    }

    /**
     * Add a {@link MessageObserver} to be notified when message from external service is changes.
     * @param observer a {@link MessageObserver} to add.
     */
    public void addObserver(MessageObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove a {@link MessageObserver}.
     * @param observer The {@link MessageObserver} to remove.
     */
    public void removeObserver(MessageObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies all {@link MessageObserver} that a message is available.
     * @param data {@link MessageData} to send to all the observers.
     */
    public void sendAvailabilityNotification(MessageData data) {
        for (MessageObserver observer : mObservers) {
            observer.messageReady(mMessageType, data);
        }
    }

    /**
     * Notifies all {@link MessageObserver} that a message is became invalid.
     */
    public void sendInvalidNotification() {
        for (MessageObserver observer : mObservers) {
            observer.messageInvalidate(mMessageType);
        }
    }
}
