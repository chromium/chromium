// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.TAB_GROUP_SUGGESTION_MESSAGE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupSuggestionMessageService.TabGroupSuggestionMessageData;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.ui.modelutil.PropertyModel;

/** A class to create the property model for the tab group suggestion message card. */
@NullMarked
public class TabGroupSuggestionMessageViewModel {
    /**
     * Creates a {@link PropertyModel} for the tab group suggestion message card.
     *
     * @param data The {@link TabGroupSuggestionMessageData} containing the dynamic strings and
     *     action providers for the message card.
     */
    public static PropertyModel create(TabGroupSuggestionMessageData data) {

        return new PropertyModel.Builder(ALL_KEYS)
                .with(MESSAGE_TYPE, TAB_GROUP_SUGGESTION_MESSAGE)
                .with(
                        MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardScope.REGULAR)
                .with(MESSAGE_IDENTIFIER, MessageService.DEFAULT_MESSAGE_IDENTIFIER)
                .with(ACTION_TEXT, data.getActionText())
                .with(UI_ACTION_PROVIDER, data.getActionProvider())
                .with(DESCRIPTION_TEXT, data.getMessageText())
                .with(DISMISS_BUTTON_CONTENT_DESCRIPTION, data.getDismissActionText())
                .with(UI_DISMISS_ACTION_PROVIDER, data.getDismissActionProvider())
                .with(SHOULD_KEEP_AFTER_REVIEW, false)
                .with(IS_CLOSE_BUTTON_VISIBLE, true)
                .with(ACTION_BUTTON_VISIBLE, true)
                .with(IS_ICON_VISIBLE, false)
                .with(IS_INCOGNITO, false)
                .with(CARD_TYPE, ModelType.MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }
}
