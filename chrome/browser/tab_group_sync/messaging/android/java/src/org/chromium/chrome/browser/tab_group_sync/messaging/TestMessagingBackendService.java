// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync.messaging;

import androidx.annotation.NonNull;

import org.chromium.components.tab_group_sync.messaging.ActivityLogItem;
import org.chromium.components.tab_group_sync.messaging.ActivityLogQueryParams;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherTabId;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService.InstantMessageDelegate;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.tab_group_sync.messaging.PersistentMessage;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

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
            EitherTabId tabId, Optional</* @PersistentNotificationType */ Integer> type) {
        return new ArrayList<PersistentMessage>();
    }

    @Override
    @NonNull
    public List<PersistentMessage> getMessagesForGroup(
            EitherGroupId groupId, Optional</* @PersistentNotificationType */ Integer> type) {
        return new ArrayList<PersistentMessage>();
    }

    @Override
    @NonNull
    public List<PersistentMessage> getMessages(
            Optional</* @PersistentNotificationType */ Integer> type) {
        return new ArrayList<PersistentMessage>();
    }

    @Override
    @NonNull
    public List<ActivityLogItem> getActivityLog(ActivityLogQueryParams params) {
        return new ArrayList<ActivityLogItem>();
    }
}
