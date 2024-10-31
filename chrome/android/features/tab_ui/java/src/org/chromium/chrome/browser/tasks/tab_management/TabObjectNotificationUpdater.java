// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;

/**
 * A partial implementation of a notifications for tab changes. Contains the pieces that all
 * concrete versions use.
 */
public abstract class TabObjectNotificationUpdater implements Destroyable {
    private final PersistentMessageObserver mPersistentMessageObserver =
            new PersistentMessageObserver() {
                @Override
                public void onMessagingBackendServiceInitialized() {
                    showAll();
                }

                @Override
                public void displayPersistentMessage(PersistentMessage message) {
                    incrementalShow(message);
                }

                @Override
                public void hidePersistentMessage(PersistentMessage message) {
                    incrementalHide(message);
                }
            };

    protected final TabListNotificationHandler mTabListNotificationHandler;
    protected final MessagingBackendService mMessagingBackendService;

    public TabObjectNotificationUpdater(
            Profile profile, TabListNotificationHandler tabListNotificationHandler) {
        mTabListNotificationHandler = tabListNotificationHandler;
        mMessagingBackendService = MessagingBackendServiceFactory.getForProfile(profile);
        mMessagingBackendService.addPersistentMessageObserver(mPersistentMessageObserver);
    }

    @Override
    public void destroy() {
        mMessagingBackendService.removePersistentMessageObserver(mPersistentMessageObserver);
    }

    /**
     * All currently applicable notifications should be pushed to the backing handler. No need to
     * clear/hide any previous message, assume everything has been wiped.
     */
    public abstract void showAll();

    /** Request that the notification for the single message be propagated. */
    protected abstract void incrementalShow(PersistentMessage message);

    /** Request that the notification for the single message stop stop showing. */
    protected abstract void incrementalHide(PersistentMessage message);
}
