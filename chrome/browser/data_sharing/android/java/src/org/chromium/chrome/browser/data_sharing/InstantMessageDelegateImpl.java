// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import androidx.core.util.Pair;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.tab_group_sync.messaging.InstantMessage;
import org.chromium.components.tab_group_sync.messaging.InstantNotificationType;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService.InstantMessageDelegate;
import org.chromium.components.tab_group_sync.messaging.UserAction;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Responsible for displaying browser and OS messages for share. This is effectively a singleton,
 * scoped by profile. This class should be attached/detached by all windows.
 */
public class InstantMessageDelegateImpl implements InstantMessageDelegate {
    private final List<Pair<MessageDispatcher, TabGroupModelFilter>> mAttachedWindows =
            new ArrayList<>();

    /**
     * @param profile The current profile to get dependencies with.
     */
    /* package */ InstantMessageDelegateImpl(Profile profile) {
        profile = profile.getOriginalProfile();
        MessagingBackendService messagingBackendService =
                MessagingBackendServiceFactory.getForProfile(profile);
        messagingBackendService.setInstantMessageDelegate(this);
    }

    /**
     * @param windowAndroid The window that can be used for showing messages.
     * @param tabGroupModelFilter The tab model and group filter for the given window.
     */
    public void attachWindow(WindowAndroid windowAndroid, TabGroupModelFilter tabGroupModelFilter) {
        MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
        mAttachedWindows.add(new Pair<>(messageDispatcher, tabGroupModelFilter));
    }

    /**
     * @param windowAndroid The window that is no longer usable for showing messages.
     */
    public void detachWindow(WindowAndroid windowAndroid) {
        MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
        mAttachedWindows.removeIf(p -> Objects.equals(p.first, messageDispatcher));
    }

    @Override
    public void displayInstantaneousMessage(InstantMessage message) {
        if (message.type == InstantNotificationType.SYSTEM) {
            // TODO(https://crbug.com/369164214): Implement.
        } else if (message.type == InstantNotificationType.BROWSER
                || message.type == InstantNotificationType.CONFLICT_TAB_REMOVED) {
            if (mAttachedWindows.size() == 0) {
                return;
            }

            @UserAction int userAction = message.action;
            if (userAction == UserAction.TAB_REMOVED) {
                // TODO(https://crbug.com/369163940): Implement.
            } else if (userAction == UserAction.TAB_NAVIGATED) {
                // TODO(https://crbug.com/369163940): Implement.
            } else if (userAction == UserAction.COLLABORATION_USER_JOINED) {
                // TODO(https://crbug.com/369163940): Implement.
            } else if (userAction == UserAction.COLLABORATION_REMOVED) {
                // TODO(https://crbug.com/369163940): Implement.
            }
        }
    }
}
