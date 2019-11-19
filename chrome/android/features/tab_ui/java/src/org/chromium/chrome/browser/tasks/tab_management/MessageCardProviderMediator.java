// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This is a {@link MessageService.MessageObserver} that creates and owns different
 * {@link PropertyModel} based on the message type.
 */
public class MessageCardProviderMediator implements MessageService.MessageObserver {
    /**
     * A class represents a Message.
     */
    public class Message {
        public final @MessageService.MessageType int type;
        public final PropertyModel model;

        Message(int type, PropertyModel model) {
            this.type = type;
            this.model = model;
        }
    }

    private Map<Integer, Message> mMessageItems = new HashMap<>();
    private Map<Integer, Message> mShownMessageItems = new HashMap<>();
    private MessageCardView.DismissActionProvider mUiDismissActionProvider;

    public MessageCardProviderMediator(
            MessageCardView.DismissActionProvider uiDismissActionProvider) {
        mUiDismissActionProvider = uiDismissActionProvider;
    }
    /**
     * @return A list of {@link Message} that can be shown.
     */
    public List<Message> getMessageItems() {
        mShownMessageItems.putAll(mMessageItems);
        mMessageItems.clear();
        return new ArrayList<>(mShownMessageItems.values());
    }

    // MessageObserver implementations.
    @Override
    public void messageReady(
            @MessageService.MessageType int type, MessageService.MessageData data) {
        assert !mShownMessageItems.containsKey(type);

        // TODO(crbug.com/1004570): Use {@code data} when ready to integrate with MessageService
        // component.
        PropertyModel model = new TabSuggestionMessageCardViewModel();
        model.set(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, mUiDismissActionProvider);
        mMessageItems.put(type, new Message(type, model));
    }

    @Override
    public void messageInvalidate(@MessageService.MessageType int type) {
        if (mMessageItems.containsKey(type)) {
            mMessageItems.remove(type);
        } else if (mShownMessageItems.containsKey(type)) {
            // run ui dismiss handler;
            MessageCardView.DismissActionProvider dismissActionProvider =
                    mShownMessageItems.get(type).model.get(
                            MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER);

            if (dismissActionProvider != null) {
                dismissActionProvider.dismiss();
            }

            mShownMessageItems.remove(type);
        }
    }

    @VisibleForTesting
    Map<Integer, Message> getReadyMessageItemsForTesting() {
        return mMessageItems;
    }

    @VisibleForTesting
    Map<Integer, Message> getShownMessageItemsForTesting() {
        return mShownMessageItems;
    }
}