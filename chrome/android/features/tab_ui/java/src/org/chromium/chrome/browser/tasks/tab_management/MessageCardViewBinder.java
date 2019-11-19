// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridSecondaryItem.
 */
class MessageCardViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        assert view instanceof MessageCardView;

        MessageCardView itemView = (MessageCardView) view;
        if (MessageCardViewProperties.ACTION_TEXT == propertyKey) {
            itemView.setActionText(model.get(MessageCardViewProperties.ACTION_TEXT));
            itemView.setActionButtonOnClickListener(v -> {
                MessageCardView.ReviewActionProvider uiProvider =
                        model.get(MessageCardViewProperties.UI_ACTION_PROVIDER);
                if (uiProvider != null) uiProvider.review();

                MessageCardView.ReviewActionProvider serviceProvider =
                        model.get(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER);
                if (serviceProvider != null) serviceProvider.review();
            });
        } else if (MessageCardViewProperties.DESCRIPTION_TEXT == propertyKey) {
            itemView.setDescriptionText(model.get(MessageCardViewProperties.DESCRIPTION_TEXT));
        } else if (MessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE == propertyKey) {
            itemView.setDescriptionTextTemplate(
                    model.get(MessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE));
        } else if (MessageCardViewProperties.ICON_PROVIDER == propertyKey) {
            itemView.setIcon(model.get(MessageCardViewProperties.ICON_PROVIDER).getIconDrawable());
        } else if (MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            itemView.setDismissButtonContentDescription(
                    model.get(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION));
            itemView.setDismissButtonOnClickListener(v -> {
                MessageCardView.DismissActionProvider uiProvider =
                        model.get(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER);
                if (uiProvider != null) uiProvider.dismiss();

                MessageCardView.DismissActionProvider serviceProvider = model.get(
                        MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER);
                if (serviceProvider != null) serviceProvider.dismiss();
            });
        }
    }
}