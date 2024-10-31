// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.collaboration.messaging.EitherId.EitherGroupId;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;

/** Pushes bubble/dot notifications for tabs. */
public class TabBubbler extends TabObjectNotificationUpdater {
    private final ObservableSupplier<Token> mTabGroupIdSupplier;

    public TabBubbler(
            Profile profile,
            TabListNotificationHandler tabListNotificationHandler,
            ObservableSupplier<Token> tabGroupIdSupplier) {
        super(profile, tabListNotificationHandler);
        mTabGroupIdSupplier = tabGroupIdSupplier;
        // Do not observe mTabGroupIdSupplier. We will be told to #showAll() is this changes.
    }

    @Override
    public void showAll() {
        @Nullable Token tabGroupId = mTabGroupIdSupplier.get();
        if (tabGroupId == null) return;
        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tabGroupId);
        EitherGroupId eitherGroupId = EitherGroupId.createLocalId(localTabGroupId);
        List<PersistentMessage> messageList =
                mMessagingBackendService.getMessagesForGroup(
                        eitherGroupId, Optional.of(PersistentNotificationType.DIRTY_TAB));

        Set<Integer> tabIds = new HashSet<>();
        for (PersistentMessage message : messageList) {
            if (shouldApply(message)) {
                tabIds.add(MessageUtils.extractTabId(message));
            }
        }
        if (!tabIds.isEmpty()) {
            mTabListNotificationHandler.updateTabStripNotificationBubble(
                    tabIds, /* hasUpdate= */ true);
        }
    }

    @Override
    protected void incrementalShow(PersistentMessage message) {
        if (shouldApply(message)) {
            Set<Integer> tabIds = Collections.singleton(MessageUtils.extractTabId(message));
            mTabListNotificationHandler.updateTabStripNotificationBubble(
                    tabIds, /* hasUpdate= */ true);
        }
    }

    @Override
    protected void incrementalHide(PersistentMessage message) {
        if (shouldApply(message)) {
            Set<Integer> tabIds = Collections.singleton(MessageUtils.extractTabId(message));
            mTabListNotificationHandler.updateTabStripNotificationBubble(
                    tabIds, /* hasUpdate= */ false);
        }
    }

    protected boolean shouldApply(PersistentMessage message) {
        return mTabGroupIdSupplier.get() != null
                && message.type == PersistentNotificationType.DIRTY_TAB
                && MessageUtils.extractTabId(message) != Tab.INVALID_TAB_ID
                && Objects.equals(
                        MessageUtils.extractTabGroupId(message), mTabGroupIdSupplier.get());
    }
}
