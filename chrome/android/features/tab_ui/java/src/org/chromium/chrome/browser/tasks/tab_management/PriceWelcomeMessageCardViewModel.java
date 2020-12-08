// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This is a util class for creating the property model of the {@link PriceWelcomeMessageCardView}.
 */
public class PriceWelcomeMessageCardViewModel {
    /**
     * Create a {@link PropertyModel} for {@link PriceWelcomeMessageCardView}.
     * @param context The {@link Context} to use.
     * @param uiDismissActionProvider The {@link MessageCardView.DismissActionProvider} to set.
     * @param data The {@link PriceWelcomeMessageService.PriceWelcomeMessageData} to use.
     * @return A {@link PropertyModel} for the given {@code data}.
     */
    public static PropertyModel create(Context context,
            MessageCardView.DismissActionProvider uiDismissActionProvider,
            PriceWelcomeMessageService.PriceWelcomeMessageData data) {
        String titleText = context.getString(R.string.price_drop_spotted_title);
        String contentText = context.getString(R.string.price_drop_spotted_content);
        String actionText = context.getString(R.string.price_drop_spotted_show_me);
        String dismissButtonContextDescription =
                context.getString(R.string.accessibility_tab_suggestion_dismiss_button);

        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(MessageCardViewProperties.MESSAGE_TYPE,
                        MessageService.MessageType.PRICE_WELCOME)
                .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, uiDismissActionProvider)
                .with(MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                        data.getDismissActionProvider())
                .with(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                        data.getReviewActionProvider())
                .with(MessageCardViewProperties.DESCRIPTION_TEXT, contentText)
                .with(MessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE, null)
                .with(MessageCardViewProperties.ACTION_TEXT, actionText)
                .with(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                        dismissButtonContextDescription)
                .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                .with(MessageCardViewProperties.IS_ICON_VISIBLE, false)
                .with(MessageCardViewProperties.IS_INCOGNITO, false)
                .with(MessageCardViewProperties.TITLE_TEXT, titleText)
                .with(MessageCardViewProperties.PRICE_DROP, data.getPriceDrop())
                .with(CARD_TYPE, MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }
}