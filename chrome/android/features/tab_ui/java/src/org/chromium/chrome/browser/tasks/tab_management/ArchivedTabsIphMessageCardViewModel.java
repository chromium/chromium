// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ICON_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope.REGULAR;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.VIEW_AS_ACTION_BUTTON;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.DEFAULT_MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.ResizableMessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.ARCHIVED_TABS_IPH_MESSAGE;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a util class for creating the property model of the IphMessageCardView. */
@NullMarked
public class ArchivedTabsIphMessageCardViewModel {
    /**
     * Create a {@link PropertyModel} for ArchivedTabsIphMessageCardView.
     *
     * @param context The {@link Context} to use.
     * @param actionProvider The provider for the review action.
     * @param serviceDismissActionProvider The provier for the dismiss action.
     * @return A {@link PropertyModel} for the ArchivedTabsIphMessageCardView.
     */
    public static PropertyModel create(
            Context context,
            ActionProvider actionProvider,
            ServiceDismissActionProvider<@MessageType Integer> serviceDismissActionProvider) {
        String dismissButtonContextDescription =
                context.getString(R.string.accessibility_tab_suggestion_dismiss_button);

        return new PropertyModel.Builder(ALL_KEYS)
                .with(MESSAGE_TYPE, ARCHIVED_TABS_IPH_MESSAGE)
                .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                .with(
                        ICON_PROVIDER,
                        (callback) -> {
                            callback.onResult(
                                    AppCompatResources.getDrawable(
                                            context, R.drawable.archived_tab_icon));
                        })
                .with(MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER, serviceDismissActionProvider)
                .with(MESSAGE_SERVICE_ACTION_PROVIDER, actionProvider)
                .with(DISMISS_BUTTON_CONTENT_DESCRIPTION, dismissButtonContextDescription)
                .with(VIEW_AS_ACTION_BUTTON, true)
                .with(ACTION_BUTTON_VISIBLE, false)
                .with(SHOULD_KEEP_AFTER_REVIEW, true)
                .with(IS_ICON_VISIBLE, true)
                .with(IS_INCOGNITO, false)
                .with(MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE, REGULAR)
                .with(CARD_TYPE, MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }
}
