// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the improved bookmarks save flow. */
class ImprovedBookmarkSaveFlowViewBinder {
    static void bind(
            PropertyModel model,
            ImprovedBookmarkSaveFlowView improvedSaveFlow,
            PropertyKey propertyKey) {
        if (propertyKey == ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_CLICK_LISTENER) {
            improvedSaveFlow.setBookmarkRowClickListener(
                    model.get(ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_CLICK_LISTENER));
        } else if (propertyKey == ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON) {
            improvedSaveFlow.setBookmarkDrawable(
                    model.get(ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON));
        } else if (propertyKey == ImprovedBookmarkSaveFlowProperties.TITLE) {
            improvedSaveFlow.setTitle(model.get(ImprovedBookmarkSaveFlowProperties.TITLE));
        } else if (propertyKey == ImprovedBookmarkSaveFlowProperties.SUBTITLE) {
            improvedSaveFlow.setSubtitle(model.get(ImprovedBookmarkSaveFlowProperties.SUBTITLE));
        } else if (propertyKey == ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE) {
            improvedSaveFlow.setPriceTrackingUiVisible(
                    model.get(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE));
        } else if (propertyKey == ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_ENABLED) {
            improvedSaveFlow.setPriceTrackingUiEnabled(
                    model.get(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_ENABLED));
        } else if (propertyKey
                == ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED) {
            improvedSaveFlow.setPriceTrackingSwitchChecked(
                    model.get(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));
        } else if (propertyKey
                == ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER) {
            improvedSaveFlow.setPriceTrackingSwitchToggleListener(
                    model.get(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER));
        }
    }
}
