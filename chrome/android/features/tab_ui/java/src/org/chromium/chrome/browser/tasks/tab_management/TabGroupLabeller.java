// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;

import java.util.List;
import java.util.Optional;

/** Pushes label updates to UI for tab groups. */
public class TabGroupLabeller extends TabObjectLabeller {
    private final ObservableSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    public TabGroupLabeller(
            Profile profile,
            TabListNotificationHandler tabListNotificationHandler,
            ObservableSupplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        super(profile, tabListNotificationHandler);
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    @Override
    protected boolean shouldApply(PersistentMessage message) {
        return mTabGroupModelFilterSupplier.get() != null
                && !mTabGroupModelFilterSupplier.get().isOffTheRecord()
                && message.type == PersistentNotificationType.DIRTY_TAB_GROUP
                && getTabId(message) != Tab.INVALID_TAB_ID;
    }

    @Override
    protected int getTextRes(PersistentMessage message) {
        return R.string.tab_group_new_activity_label;
    }

    @Override
    protected List<PersistentMessage> getAllMessages() {
        return mMessagingBackendService.getMessages(
                Optional.of(PersistentNotificationType.DIRTY_TAB_GROUP));
    }

    @Override
    protected int getTabId(PersistentMessage message) {
        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        if (tabGroupId == null) {
            return Tab.INVALID_TAB_ID;
        } else {
            return mTabGroupModelFilterSupplier.get().getRootIdFromStableId(tabGroupId);
        }
    }
}
