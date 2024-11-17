// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.EitherId.EitherGroupId;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Optional;

/** Pushes label updates to UI for tabs. */
public class TabLabeller extends TabObjectLabeller {
    private final ObservableSupplier<Token> mTabGroupIdSupplier;

    public TabLabeller(
            Profile profile,
            TabListNotificationHandler tabListNotificationHandler,
            ObservableSupplier<Token> tabGroupIdSupplier) {
        super(profile, tabListNotificationHandler);
        mTabGroupIdSupplier = tabGroupIdSupplier;
        // Do not observe mTabGroupIdSupplier. We will be told to #showAll() is this changes.
    }

    @Override
    protected boolean shouldApply(PersistentMessage message) {
        return mTabGroupIdSupplier.get() != null
                && Objects.equals(
                        mTabGroupIdSupplier.get(), MessageUtils.extractTabGroupId(message))
                && message.type == PersistentNotificationType.CHIP
                && getTabId(message) != Tab.INVALID_TAB_ID
                && getTextRes(message) != Resources.ID_NULL;
    }

    @Override
    protected int getTextRes(PersistentMessage message) {
        if (message.collaborationEvent == CollaborationEvent.TAB_ADDED) {
            return org.chromium.chrome.tab_ui.R.string.tab_added_label;
        } else if (message.collaborationEvent == CollaborationEvent.TAB_UPDATED) {
            return org.chromium.chrome.tab_ui.R.string.tab_changed_label;
        } else {
            return Resources.ID_NULL;
        }
    }

    @Override
    protected List<PersistentMessage> getAllMessages() {
        @Nullable Token tabGroupId = mTabGroupIdSupplier.get();
        if (tabGroupId == null) return Collections.emptyList();
        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tabGroupId);
        EitherGroupId eitherGroupId = EitherGroupId.createLocalId(localTabGroupId);
        Optional<Integer> messageType = Optional.of(PersistentNotificationType.CHIP);
        return mMessagingBackendService.getMessagesForGroup(eitherGroupId, messageType);
    }

    @Override
    protected int getTabId(PersistentMessage message) {
        return MessageUtils.extractTabId(message);
    }

    @Override
    protected AsyncImageView.Factory getAsyncImageFactory(PersistentMessage message) {
        // TODO(https://crbug.com/369188289): Fetch from message.attribution.triggeringUser.
        return null;
    }
}
