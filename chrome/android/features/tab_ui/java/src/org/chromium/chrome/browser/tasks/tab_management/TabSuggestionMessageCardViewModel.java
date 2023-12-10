// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;

import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a util class for creating the property model of the TabSuggestionMessageCard. */
public class TabSuggestionMessageCardViewModel {
    /**
     * Create a {@link PropertyModel} for TabSuggestionMessageCardView.
     * @param context The {@link Context} to use.
     * @param uiDismissActionProvider The {@link MessageCardView.DismissActionProvider} to set.
     * @param data The {@link TabSuggestionMessageService.TabSuggestionMessageData} to use.
     * @return A {@link PropertyModel} for the given {@code data}.
     */
    public static PropertyModel create(
            Context context,
            MessageCardView.DismissActionProvider uiDismissActionProvider,
            TabSuggestionMessageService.TabSuggestionMessageData data) {
        // TODO(crbug.com/1487664): Add any missing accessibility or button descriptions.
        String titleText = getTitleText(context, data.getActionType());
        String descriptionText = getDescriptionText(context, data);
        String actionText = getActionText(context, data.getActionType());
        String secondaryActionText = getSecondaryActionText(context, data.getActionType());
        int iconWidth = getIconWidth(context, data.getActionType());
        int iconHeight = getIconHeight(context, data.getActionType());
        String dismissButtonContextDescription =
                context.getString(R.string.accessibility_tab_suggestion_dismiss_button);

        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(
                        MessageCardViewProperties.MESSAGE_TYPE,
                        MessageService.MessageType.TAB_SUGGESTION)
                .with(MessageCardViewProperties.MESSAGE_IDENTIFIER, data.getActionType())
                .with(MessageCardViewProperties.ICON_WIDTH_IN_PIXELS, iconWidth)
                .with(MessageCardViewProperties.ICON_HEIGHT_IN_PIXELS, iconHeight)
                .with(
                        MessageCardViewProperties.ICON_PROVIDER,
                        data.createMultiFaviconIconProvider(context))
                .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, uiDismissActionProvider)
                .with(
                        MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                        data.getDismissActionProvider())
                .with(
                        MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                        data.getReviewActionProvider())
                .with(MessageCardViewProperties.TITLE_TEXT, titleText)
                .with(MessageCardViewProperties.DESCRIPTION_TEXT, descriptionText)
                .with(MessageCardViewProperties.ACTION_TEXT, actionText)
                .with(MessageCardViewProperties.SECONDARY_ACTION_TEXT, secondaryActionText)
                .with(
                        MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                        dismissButtonContextDescription)
                .with(MessageCardViewProperties.IS_ICON_VISIBLE, true)
                .with(MessageCardViewProperties.IS_INCOGNITO, false)
                .with(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardViewProperties.MessageCardScope.REGULAR)
                .with(CARD_TYPE, MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }

    private static String getTitleText(
            Context context, @TabSuggestion.TabSuggestionAction int suggestionActionType) {
        switch (suggestionActionType) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return context.getString(R.string.tab_cleanup_message_card_title);
            default:
                assert false : "Invalid TabSuggestionAction";
                return "";
        }
    }

    private static String getDescriptionText(
            Context context, TabSuggestionMessageService.TabSuggestionMessageData data) {
        int suggestionActionType = data.getActionType();

        switch (suggestionActionType) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return context.getResources()
                        .getQuantityString(
                                R.plurals.tab_cleanup_message_card_subtitle,
                                data.getSize(),
                                data.getSize());
            default:
                assert false : "Invalid TabSuggestionAction";
                return "";
        }
    }

    private static String getActionText(
            Context context, @TabSuggestion.TabSuggestionAction int suggestionActionType) {
        switch (suggestionActionType) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return context.getString(R.string.tab_cleanup_message_card_review_tabs_button);
            default:
                assert false : "Invalid TabSuggestionAction";
                return "";
        }
    }

    private static String getSecondaryActionText(
            Context context, @TabSuggestion.TabSuggestionAction int suggestionActionType) {
        switch (suggestionActionType) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return context.getString(R.string.tab_cleanup_message_card_close_tabs_button);
            default:
                assert false : "Invalid TabSuggestionAction";
                return "";
        }
    }

    private static int getIconWidth(
            Context context, @TabSuggestion.TabSuggestionAction int suggestionActionType) {
        switch (suggestionActionType) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return (int)
                        context.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_width);
            default:
                assert false : "Invalid TabSuggestionAction";
                return 0;
        }
    }

    private static int getIconHeight(
            Context context, @TabSuggestion.TabSuggestionAction int suggestionActionType) {
        switch (suggestionActionType) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return (int)
                        context.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_height);
            default:
                assert false : "Invalid TabSuggestionAction";
                return 0;
        }
    }
}
