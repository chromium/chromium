// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.labels;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListNotificationHandler;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;

import java.util.List;
import java.util.Map;

/** Pushes label updates to UI for tab groups. */
@NullMarked
public class TabGroupLabeller extends TabObjectLabeller<Token> {
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
        Token tabGroupId = MessageUtils.extractTabGroupId(message);
        return tabModel != null
                && !tabModel.isOffTheRecord()
                && message.type == PersistentNotificationType.DIRTY_TAB_GROUP
                && tabGroupId != null
                && tabModel.containsTabGroup(tabGroupId);
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
    protected Token getKey(PersistentMessage message) {
        return assumeNonNull(MessageUtils.extractTabGroupId(message));
    }

    @Override
    protected void applyLabels(Map<Token, TabCardLabelData> cardLabels) {
        mTabListNotificationHandler.updateTabGroupCardLabels(cardLabels);
    }
}
