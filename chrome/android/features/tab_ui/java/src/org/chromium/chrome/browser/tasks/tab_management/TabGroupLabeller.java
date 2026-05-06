// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;

import java.util.List;

/** Pushes label updates to UI for tab groups. */
@NullMarked
public class TabGroupLabeller extends TabObjectLabeller {
    private final NullableObservableSupplier<TabModel> mTabModelSupplier;

    public TabGroupLabeller(
            Profile profile,
            TabListNotificationHandler tabListNotificationHandler,
            NullableObservableSupplier<TabModel> tabModelSupplier) {
        super(profile, tabListNotificationHandler);
        mTabModelSupplier = tabModelSupplier;
    }

    @Override
    protected boolean shouldApply(PersistentMessage message) {
        TabModel tabModel = mTabModelSupplier.get();
        return tabModel != null
                && !tabModel.isOffTheRecord()
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
            TabModel tabModel = mTabModelSupplier.get();
            assumeNonNull(tabModel);
            return tabModel.getGroupLastShownTabId(tabGroupId);
        }
    }
}
