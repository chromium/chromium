// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for TabGridLargeMessageItem. */
class LargeMessageCardViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        assert view instanceof LargeMessageCardView;

        LargeMessageCardView itemView = (LargeMessageCardView) view;
        if (MessageCardViewProperties.ACTION_TEXT == propertyKey) {
            itemView.setActionText(model.get(MessageCardViewProperties.ACTION_TEXT));
            itemView.setActionButtonOnClickListener(
                    v -> {
                        LargeMessageCardViewBinder.handleReviewActionButton(model);
                    });
        } else if (MessageCardViewProperties.TITLE_TEXT == propertyKey) {
            itemView.setTitleText(model.get(MessageCardViewProperties.TITLE_TEXT));
        } else if (MessageCardViewProperties.DESCRIPTION_TEXT == propertyKey) {
            itemView.setDescriptionText(model.get(MessageCardViewProperties.DESCRIPTION_TEXT));
        } else if (MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            itemView.setDismissButtonContentDescription(
                    model.get(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION));
            itemView.setDismissButtonOnClickListener(
                    v -> {
                        LargeMessageCardViewBinder.handleDismissActionButton(model);
                    });
        } else if (MessageCardViewProperties.SECONDARY_ACTION_TEXT == propertyKey) {
            itemView.setSecondaryActionText(
                    model.get(MessageCardViewProperties.SECONDARY_ACTION_TEXT));
        } else if (MessageCardViewProperties.SECONDARY_ACTION_BUTTON_CLICK_HANDLER == propertyKey) {
            itemView.setSecondaryActionButtonOnClickListener(
                    model.get(MessageCardViewProperties.SECONDARY_ACTION_BUTTON_CLICK_HANDLER));
        } else if (MessageCardViewProperties.PRICE_DROP == propertyKey) {
            itemView.setupPriceInfoBox(model.get(MessageCardViewProperties.PRICE_DROP));
        } else if (MessageCardViewProperties.ICON_PROVIDER == propertyKey) {
            updateIconDrawable(model, itemView);
        } else if (MessageCardViewProperties.IS_ICON_VISIBLE == propertyKey) {
            itemView.setIconVisibility(model.get(MessageCardViewProperties.IS_ICON_VISIBLE));
        } else if (CARD_ALPHA == propertyKey) {
            itemView.setAlpha(model.get(CARD_ALPHA));
        } else if (MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE == propertyKey) {
            itemView.setCloseButtonVisibility(
                    model.get(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE));
        } else if (MessageCardViewProperties.IS_INCOGNITO == propertyKey) {
            itemView.updateMessageCardColor(model.get(MessageCardViewProperties.IS_INCOGNITO));
        } else if (MessageCardViewProperties.ICON_WIDTH_IN_PIXELS == propertyKey) {
            itemView.updateIconWidth(model.get(MessageCardViewProperties.ICON_WIDTH_IN_PIXELS));
        } else if (MessageCardViewProperties.ICON_HEIGHT_IN_PIXELS == propertyKey) {
            itemView.updateIconHeight(model.get(MessageCardViewProperties.ICON_HEIGHT_IN_PIXELS));
        }
    }

    @VisibleForTesting
    static void updateIconDrawable(PropertyModel model, LargeMessageCardView itemView) {
        MessageCardView.IconProvider provider = model.get(MessageCardViewProperties.ICON_PROVIDER);

        if (provider != null) {
            provider.fetchIconDrawable(
                    (drawable) -> {
                        itemView.setIconDrawable(drawable);
                    });
        }
    }

    @VisibleForTesting
    static void handleDismissActionButton(PropertyModel model) {
        int type = model.get(MessageCardViewProperties.MESSAGE_TYPE);
        MessageCardView.DismissActionProvider uiProvider =
                model.get(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER);
        if (uiProvider != null) uiProvider.dismiss(type);
        MessageCardView.DismissActionProvider serviceProvider =
                model.get(MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER);
        if (serviceProvider != null) serviceProvider.dismiss(type);
    }

    @VisibleForTesting
    static void handleReviewActionButton(PropertyModel model) {
        MessageCardView.ReviewActionProvider uiProvider =
                model.get(MessageCardViewProperties.UI_ACTION_PROVIDER);
        if (uiProvider != null) uiProvider.review();

        MessageCardView.ReviewActionProvider serviceProvider =
                model.get(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER);
        if (serviceProvider != null) serviceProvider.review();

        MessageCardView.DismissActionProvider uiDismissProvider =
                model.get(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER);
        if (uiDismissProvider != null
                && !model.get(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW)) {
            uiDismissProvider.dismiss(model.get(MessageCardViewProperties.MESSAGE_TYPE));
        }
    }
}
