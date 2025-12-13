// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ICON_HIGHLIGHTED;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.WIDTH;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope.REGULAR;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ANIMATION_STATUS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.ARCHIVED_TABS_MESSAGE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsMessageService.ArchivedTabsMessageData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the custom view for archived tabs. */
@NullMarked
public class ArchivedTabsCardViewBinder {
    /**
     * Binder method for the archived tabs custom message
     *
     * @param model The {@link PropertyModel} for the view.
     * @param view The {@link View} to bind.
     * @param key The {@link PropertyKey} to bind.
     */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        ArchivedTabsCardView cardView = (ArchivedTabsCardView) view;
        if (key == NUMBER_OF_ARCHIVED_TABS) {
            cardView.setNumberOfArchivedTabs(model.get(NUMBER_OF_ARCHIVED_TABS));
        } else if (key == ICON_HIGHLIGHTED) {
            cardView.setIconHighlight(model.get(ICON_HIGHLIGHTED));
        } else if (key == CLICK_HANDLER) {
            cardView.setClickHandler(model.get(CLICK_HANDLER));
        } else if (key == WIDTH) {
            cardView.setCardWidth(model.get(WIDTH));
        } else if (key == CARD_ALPHA) {
            cardView.setAlpha(model.get(CARD_ALPHA));
        } else if (key == CARD_ANIMATION_STATUS) {
            cardView.scaleCard(model.get(CARD_ANIMATION_STATUS));
        }
    }

    public static PropertyModel createPropertyModel(ArchivedTabsMessageData data) {
        return new PropertyModel.Builder(ArchivedTabsCardViewProperties.ALL_KEYS)
                .with(CLICK_HANDLER, data.onClickRunnable)
                .with(ICON_HIGHLIGHTED, false)
                .with(MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE, REGULAR)
                .with(CARD_ALPHA, 1f)
                .with(MESSAGE_TYPE, ARCHIVED_TABS_MESSAGE)
                .with(CARD_TYPE, MESSAGE)
                .build();
    }
}
