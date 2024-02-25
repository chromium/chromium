// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for CustomMessageCardItem. */
public class CustomMessageCardViewBinder {
    static class ViewHolder {
        final CustomMessageCardView mContentView;
        final CustomMessageCardProvider mProvider;

        public ViewHolder(CustomMessageCardView contentView, CustomMessageCardProvider provider) {
            mContentView = contentView;
            mProvider = provider;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (CustomMessageCardViewProperties.MESSAGE_CARD_VIEW == propertyKey) {
            viewHolder.mContentView.setChildView(
                    model.get(CustomMessageCardViewProperties.MESSAGE_CARD_VIEW));
        } else if (CARD_ALPHA == propertyKey) {
            viewHolder.mContentView.setAlpha(model.get(CARD_ALPHA));
        } else if (MessageCardViewProperties.IS_INCOGNITO == propertyKey) {
            viewHolder.mProvider.setIsIncognito(model.get(MessageCardViewProperties.IS_INCOGNITO));
        }
    }
}
