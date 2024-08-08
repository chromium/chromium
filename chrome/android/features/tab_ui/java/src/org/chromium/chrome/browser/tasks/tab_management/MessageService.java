// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Ideally, for each of the {@link MessageType} requires a MessageService class. This is the base
 * class. All the concrete subclass should contain logic that convert the data from the
 * corresponding external service to a data structure that the TabGridMessageCardProvider
 * understands.
 */
public class MessageService {
    @IntDef({
        MessageType.IPH,
        MessageType.PRICE_MESSAGE,
        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
        MessageType.ARCHIVED_TABS_MESSAGE,
        MessageType.ARCHIVED_TABS_IPH_MESSAGE,
        MessageType.COLLABORATION_ACTIVITY,
        MessageType.ALL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageType {
        int FOR_TESTING = 0;
        int IPH = 1;
        int PRICE_MESSAGE = 2;
        int INCOGNITO_REAUTH_PROMO_MESSAGE = 3;
        int ARCHIVED_TABS_MESSAGE = 4;
        int ARCHIVED_TABS_IPH_MESSAGE = 5;
        int COLLABORATION_ACTIVITY = 6;
        int ALL = 7;
    }

    /**
     * The reason why we disable the message in grid tab switcher and no longer show it.
     *
     * <p>Needs to stay in sync with GridTabSwitcherMessageDisableReason in enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        MessageDisableReason.UNKNOWN,
        MessageDisableReason.MESSAGE_ACCEPTED,
        MessageDisableReason.MESSAGE_DISMISSED,
        MessageDisableReason.MESSAGE_IGNORED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageDisableReason {
        int UNKNOWN = 0;
        // User accepts the message by tapping the primary button on it.
        int MESSAGE_ACCEPTED = 1;
        // User dismisses the message by tapping the close button on it.
        int MESSAGE_DISMISSED = 2;
        // We no longer show the message because the message is ignored by users many times.
        int MESSAGE_IGNORED = 3;
        // Always update MAX_VALUE to match the last item in the list.
        int MAX_VALUE = 3;
    }

    // This identifier is used to serve messages that have no subtype, such as IPH. If one message
    // type has multiple subtypes such as PRICE_MESSAGE, its service needs to define its own
    // identifiers which should be used when creating the message card view model.
    public static final int DEFAULT_MESSAGE_IDENTIFIER = -1;

    /**
     * This is a data wrapper. Implement this interface to send notification with data to all the
     * observers.
     *
     * @see #sendAvailabilityNotification(MessageData).
     */
    public interface MessageData {}

    /**
     * Extends {@link MessageData} for CUSTOM_MESSAGE types which require a {@link
     * CustomMessageCardProvider}.
     */
    public interface CustomMessageData extends MessageData {
        /** Returns a provider of information used for custom messages. */
        CustomMessageCardProvider getProvider();
    }

    /**
     * An interface to be notified about changes to a Message. TODO(meiliang): Need to define this
     * interface in more detail.
     */
    public interface MessageObserver {
        /**
         * Called when a message is available. TODO(meiliang): message data is needed.
         *
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
    @MessageType int mMessageType;

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

    protected ObserverList<MessageObserver> getObserversForTesting() {
        return mObservers;
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

    /** Notifies all {@link MessageObserver} that a message was invalidated. */
    public void sendInvalidNotification() {
        for (MessageObserver observer : mObservers) {
            observer.messageInvalidate(mMessageType);
        }
    }

    /**
     * Log metrics related to the message disable reason.
     * @param messageType the message type or identifier.
     * @param reason the message disable reason.
     */
    void logMessageDisableMetrics(String messageType, @MessageDisableReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                String.format("GridTabSwitcher.%s.DisableReason", messageType),
                reason,
                MessageDisableReason.MAX_VALUE + 1);
    }
}
