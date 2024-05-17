// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for CustomMessageCardItem. */
public class CustomMessageCardViewBinder {
    public static void bind(
            PropertyModel model, CustomMessageCardView view, PropertyKey propertyKey) {
        if (CustomMessageCardViewProperties.CUSTOM_VIEW == propertyKey) {
            view.setChildView(model.get(CustomMessageCardViewProperties.CUSTOM_VIEW));
        } else if (CARD_ALPHA == propertyKey) {
            view.setAlpha(model.get(CARD_ALPHA));
        } else if (MessageCardViewProperties.IS_INCOGNITO == propertyKey) {
            model.get(CustomMessageCardViewProperties.IS_INCOGNITO_CALLBACK)
                    .onResult(model.get(MessageCardViewProperties.IS_INCOGNITO));
        }
    }
}
