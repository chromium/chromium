// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.DismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ReviewActionProvider;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a util class for creating the property model of the IphMessageCardView. */
public class ArchivedTabsIphMessageCardViewModel {
    /**
     * Create a {@link PropertyModel} for ArchivedTabsIphMessageCardView.
     *
     * @param context The {@link Context} to use.
     * @param reviewActionProvider The provider for the review action.
     * @param dismissActionProvider The provier for the dismiss action.
     * @return A {@link PropertyModel} for the ArchivedTabsIphMessageCardView.
     */
    public static PropertyModel create(
            Context context,
            ReviewActionProvider reviewActionProvider,
            DismissActionProvider dismissActionProvider) {
        String dismissButtonContextDescription =
                context.getString(R.string.accessibility_tab_suggestion_dismiss_button);

        return new PropertyModel.Builder(ResizableMessageCardViewProperties.ALL_KEYS)
                .with(
                        MessageCardViewProperties.MESSAGE_TYPE,
                        MessageService.MessageType.ARCHIVED_TABS_IPH_MESSAGE)
                .with(
                        MessageCardViewProperties.MESSAGE_IDENTIFIER,
                        MessageService.DEFAULT_MESSAGE_IDENTIFIER)
                .with(
                        MessageCardViewProperties.ICON_PROVIDER,
                        (callback) -> {
                            callback.onResult(
                                    AppCompatResources.getDrawable(
                                            context, R.drawable.archived_tab_icon));
                        })
                .with(
                        MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                        dismissActionProvider)
                .with(
                        MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                        reviewActionProvider)
                .with(
                        MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                        dismissButtonContextDescription)
                .with(MessageCardViewProperties.VIEW_AS_ACTION_BUTTON, true)
                .with(MessageCardViewProperties.ACTION_BUTTON_VISIBLE, false)
                .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, true)
                .with(MessageCardViewProperties.IS_ICON_VISIBLE, true)
                .with(MessageCardViewProperties.IS_INCOGNITO, false)
                .with(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardViewProperties.MessageCardScope.BOTH)
                .with(CARD_TYPE, MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }
}
