// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ANIMATION_STATUS;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for CustomMessageCardItem. */
@NullMarked
public class CustomMessageCardViewBinder {
    public static void bind(
            PropertyModel model, CustomMessageCardView view, PropertyKey propertyKey) {
        if (CustomMessageCardViewProperties.CUSTOM_VIEW == propertyKey) {
            // Before attaching the view remove it from its parent. It may still be attached to a
            // previous parent instance of CustomMessageCardView.
            View customView = model.get(CustomMessageCardViewProperties.CUSTOM_VIEW);
            if (customView.getParent() instanceof ViewGroup group) {
                group.removeView(customView);
            }
            view.setChildView(customView);
        } else if (CARD_ALPHA == propertyKey) {
            view.setAlpha(model.get(CARD_ALPHA));
        } else if (CARD_ANIMATION_STATUS == propertyKey) {
            view.scaleCard(model.get(CARD_ANIMATION_STATUS));
        } else if (MessageCardViewProperties.IS_INCOGNITO == propertyKey) {
            Callback<Boolean> callback =
                    model.get(CustomMessageCardViewProperties.IS_INCOGNITO_CALLBACK);
            if (callback != null) {
                callback.onResult(model.get(MessageCardViewProperties.IS_INCOGNITO));
            }
        }
    }
}
