// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the coordinator for MessageCardProvider component. This component is used to build a
 * TabGridMessageCardView for each {@link MessageService.MessageType}. This coordinator manages the
 * life-cycle of all shared components and the connection between all subscribed
 * {@link MessageService}.
 */
public class MessageCardProviderCoordinator {
    private final MessageCardProviderMediator mMediator;
    private final List<MessageService> mMessageServices = new ArrayList<>();

    MessageCardProviderCoordinator(MessageCardView.DismissActionProvider uiDismissActionProvider) {
        mMediator = new MessageCardProviderMediator(uiDismissActionProvider);
    }

    /**
     * Subscribes to a {@link MessageService} to get any message changes. @see
     * MessageObserver.
     * @param service The {@link MessageService} to subscribe.
     */
    public void subscribeMessageService(MessageService service) {
        mMessageServices.add(service);
        service.addObserver(mMediator);
    }

    /**
     * Get all messages.
     * @return a list of {@link
     *         MessageCardProviderMediator.Message}.
     */
    public List<MessageCardProviderMediator.Message> getMessageItems() {
        return mMediator.getMessageItems();
    }

    /**
     * Clean up all member fields.
     */
    public void destroy() {
        for (int i = 0; i < mMessageServices.size(); i++) {
            mMessageServices.get(i).removeObserver(mMediator);
        }
    }
}
