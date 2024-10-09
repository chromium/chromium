// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a util class for creating the property model of the PriceMessageCard. */
public class PriceMessageCardViewModel {
    /**
     * Create a {@link PropertyModel} for PriceMessageCardView.
     *
     * @param context The {@link Context} to use.
     * @param uiDismissActionProvider The {@link MessageCardView.DismissActionProvider} to set.
     * @param data The {@link PriceMessageService.PriceMessageData} to use.
     * @param notificationManager The {@link PriceDropNotificationManager} handling notifications.
     * @return A {@link PropertyModel} for the given {@code data}.
     */
    public static PropertyModel create(
            Context context,
            MessageCardView.DismissActionProvider uiDismissActionProvider,
            PriceMessageService.PriceMessageData data,
            PriceDropNotificationManager notificationManager) {
        boolean isIconVisible = data.getType() != PriceMessageType.PRICE_WELCOME;
        String titleText = getTitle(context, data.getType());
        String descriptionText = getDescription(context, data.getType(), notificationManager);
        String actionText = getActionText(context, data.getType(), notificationManager);
        String dismissButtonContextDescription =
                context.getString(R.string.accessibility_tab_suggestion_dismiss_button);

        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(
                        MessageCardViewProperties.MESSAGE_TYPE,
                        MessageService.MessageType.PRICE_MESSAGE)
                .with(MessageCardViewProperties.MESSAGE_IDENTIFIER, data.getType())
                .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, uiDismissActionProvider)
                .with(
                        MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                        data.getDismissActionProvider())
                .with(
                        MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                        data.getReviewActionProvider())
                .with(MessageCardViewProperties.DESCRIPTION_TEXT, descriptionText)
                .with(MessageCardViewProperties.ACTION_TEXT, actionText)
                .with(
                        MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                        dismissButtonContextDescription)
                .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                .with(MessageCardViewProperties.IS_ICON_VISIBLE, isIconVisible)
                .with(MessageCardViewProperties.IS_INCOGNITO, false)
                .with(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardViewProperties.MessageCardScope.REGULAR)
                .with(MessageCardViewProperties.TITLE_TEXT, titleText)
                .with(MessageCardViewProperties.PRICE_DROP, data.getPriceDrop())
                .with(
                        MessageCardViewProperties.ICON_PROVIDER,
                        (callback) -> {
                            callback.onResult(getIconDrawable(context, data.getType()));
                        })
                .with(CARD_TYPE, MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }

    private static String getTitle(Context context, @PriceMessageType int type) {
        if (type == PriceMessageType.PRICE_WELCOME) {
            return context.getString(R.string.price_drop_spotted_title);
        }
        return null;
    }

    private static String getDescription(
            Context context,
            @PriceMessageType int type,
            PriceDropNotificationManager notificationManager) {
        if (type == PriceMessageType.PRICE_WELCOME) {
            return context.getString(R.string.price_drop_spotted_content);
        }
        return null;
    }

    private static String getActionText(
            Context context,
            @PriceMessageType int type,
            PriceDropNotificationManager notificationManager) {
        if (type == PriceMessageType.PRICE_WELCOME) {
            return context.getString(R.string.price_drop_spotted_show_me);
        }
        return null;
    }

    private static Drawable getIconDrawable(Context context, @PriceMessageType int type) {
        return null;
    }
}
