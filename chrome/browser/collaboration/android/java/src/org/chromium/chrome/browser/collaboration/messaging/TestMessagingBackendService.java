// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration.messaging;

import androidx.annotation.NonNull;

import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.ActivityLogQueryParams;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.EitherId.EitherTabId;

import java.util.ArrayList;
import java.util.List;

/** Test implementation of the MessagingBackendService. */
class TestMessagingBackendService implements MessagingBackendService {
    @Override
    public void setInstantMessageDelegate(InstantMessageDelegate delegate) {}

    @Override
    public void addPersistentMessageObserver(PersistentMessageObserver observer) {}

    @Override
    public void removePersistentMessageObserver(PersistentMessageObserver observer) {}

    @Override
    public boolean isInitialized() {
        return false;
    }

    @Override
    @NonNull
    public List<PersistentMessage> getMessagesForTab(
            EitherTabId tabId, @PersistentNotificationType int type) {
        return new ArrayList<>();
    }

    @Override
    @NonNull
    public List<PersistentMessage> getMessagesForGroup(
            EitherGroupId groupId, @PersistentNotificationType int type) {
        return new ArrayList<>();
    }

    @Override
    @NonNull
    public List<PersistentMessage> getMessages(@PersistentNotificationType int type) {
        return new ArrayList<>();
    }

    @Override
    @NonNull
    public List<ActivityLogItem> getActivityLog(ActivityLogQueryParams params) {
        return new ArrayList<>();
    }

    @Override
    public void clearDirtyTabMessagesForGroup(String collaborationId) {}

    @Override
    public void clearPersistentMessage(String messageId, @PersistentNotificationType int type) {}
}
