// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;

import java.util.List;

/** Pushes label updates to UI for tab groups. */
@NullMarked
public class TabGroupLabeller extends TabObjectLabeller {
    private final ObservableSupplier<@Nullable TabGroupModelFilter> mTabGroupModelFilterSupplier;

    public TabGroupLabeller(
            Profile profile,
            TabListNotificationHandler tabListNotificationHandler,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier) {
        super(profile, tabListNotificationHandler);
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    @Override
    protected boolean shouldApply(PersistentMessage message) {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        return filter != null
                && !filter.getTabModel().isOffTheRecord()
                && message.type == PersistentNotificationType.DIRTY_TAB_GROUP
                && getTabId(message) != Tab.INVALID_TAB_ID;
    }

    @Override
    protected int getTextRes(PersistentMessage message) {
        return R.string.tab_group_new_activity_label;
    }

    @Override
    protected List<PersistentMessage> getAllMessages() {
        return mMessagingBackendService.getMessages(PersistentNotificationType.DIRTY_TAB_GROUP);
    }

    @Override
    protected int getTabId(PersistentMessage message) {
        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        if (tabGroupId == null) {
            return Tab.INVALID_TAB_ID;
        } else {
            // Tabs in the TabListMediator are represented by the last shown tab ID in a tab group.
            // This is a workaround to achieve compatibility. Longer term, TabListMediator needs to
            // be refactored to accept either rootId or even better tabGroupId as the identifier for
            // tab groups. See https://crbug.com/387509285.
            TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
            assumeNonNull(filter);
            return filter.getGroupLastShownTabId(tabGroupId);
        }
    }
}
