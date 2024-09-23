// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.VIEW_AS_ACTION_BUTTON;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.DEFAULT_MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.PluralsRes;

import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.DismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Wrapper around a {@link PropertyModel} for the activity collaboration message card. */
public class CollaborationActivityMessageCardViewModel {
    private final PropertyModel mPropertyModel;

    /**
     * @param context The {@link Context} to use.
     * @param reviewActionProvider The provider for the review action.
     * @param dismissActionProvider The provier for the dismiss action.
     */
    public CollaborationActivityMessageCardViewModel(
            Context context,
            ReviewActionProvider reviewActionProvider,
            DismissActionProvider dismissActionProvider) {
        String dismissButtonContentDescription =
                context.getString(R.string.accessibility_tab_suggestion_dismiss_button);
        String actionText =
                context.getString(R.string.tab_grid_dialog_collaboration_activity_action_text);
        mPropertyModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(CARD_TYPE, MESSAGE)
                        .with(MESSAGE_TYPE, MessageType.COLLABORATION_ACTIVITY)
                        .with(CARD_ALPHA, 1f)
                        .with(ACTION_TEXT, actionText)
                        .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                        .with(MESSAGE_SERVICE_ACTION_PROVIDER, reviewActionProvider)
                        .with(MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER, dismissActionProvider)
                        .with(DISMISS_BUTTON_CONTENT_DESCRIPTION, dismissButtonContentDescription)
                        .with(VIEW_AS_ACTION_BUTTON, false)
                        .with(ACTION_BUTTON_VISIBLE, true)
                        .with(SHOULD_KEEP_AFTER_REVIEW, true)
                        .with(IS_ICON_VISIBLE, false)
                        .with(IS_INCOGNITO, false)
                        .build();
    }

    /** Returns the property model. */
    public PropertyModel getPropertyModel() {
        return mPropertyModel;
    }

    /**
     * Updates the description text.
     *
     * @param context The {@link Context} to use.
     * @param tabsAdded The number of tabs added.
     * @param tabsChanged The number of tabs changed.
     * @param tabsClosed The number of tabs closed.
     */
    public void updateDescriptionText(
            Context context, int tabsAdded, int tabsChanged, int tabsClosed) {
        PluralData pluralData = getPluralData(tabsAdded, tabsChanged, tabsClosed);

        String descriptionText;
        if (pluralData.id == Resources.ID_NULL) {
            descriptionText =
                    context.getString(R.string.tab_grid_dialog_collaboration_activity_no_updates);
        } else {
            descriptionText =
                    context.getResources()
                            .getQuantityString(
                                    pluralData.id,
                                    pluralData.quantity,
                                    tabsAdded,
                                    tabsChanged,
                                    tabsClosed);
        }

        mPropertyModel.set(DESCRIPTION_TEXT, descriptionText);
    }

    private static class PluralData {
        public @PluralsRes int id = Resources.ID_NULL;
        public int quantity;
    }

    private PluralData getPluralData(int tabsAdded, int tabsChanged, int tabsClosed) {
        PluralData pluralData = new PluralData();
        if (tabsAdded > 0) {
            pluralData.quantity = tabsAdded;
            if (tabsChanged > 0) {
                if (tabsClosed > 0) {
                    pluralData.id =
                            R.plurals
                                    .tab_grid_dialog_collaboration_activity_tabs_added_changed_closed;
                    return pluralData;
                }

                pluralData.id = R.plurals.tab_grid_dialog_collaboration_activity_tabs_added_changed;
                return pluralData;
            }

            if (tabsClosed > 0) {
                pluralData.id = R.plurals.tab_grid_dialog_collaboration_activity_tabs_added_closed;
                return pluralData;
            }

            pluralData.id = R.plurals.tab_grid_dialog_collaboration_activity_tabs_added;
            return pluralData;
        }

        if (tabsChanged > 0) {
            pluralData.quantity = tabsChanged;
            if (tabsClosed > 0) {
                pluralData.id =
                        R.plurals.tab_grid_dialog_collaboration_activity_tabs_changed_closed;
                return pluralData;
            }

            pluralData.id = R.plurals.tab_grid_dialog_collaboration_activity_tabs_changed;
            return pluralData;
        }

        if (tabsClosed > 0) {
            pluralData.quantity = tabsClosed;
            pluralData.id = R.plurals.tab_grid_dialog_collaboration_activity_tabs_closed;
            return pluralData;
        }

        return pluralData;
    }
}
