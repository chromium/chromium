// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class FeedManagementItemViewBinder {
    public static void bind(
            PropertyModel model, FeedManagementItemView view, PropertyKey propertyKey) {
        if (FeedManagementItemProperties.TITLE_KEY == propertyKey) {
            view.setTitle(model.get(FeedManagementItemProperties.TITLE_KEY));
        } else if (FeedManagementItemProperties.DESCRIPTION_KEY == propertyKey) {
            view.setDescription(model.get(FeedManagementItemProperties.DESCRIPTION_KEY));
        } else if (FeedManagementItemProperties.ON_CLICK_KEY == propertyKey) {
            view.setOnClickListener(model.get(FeedManagementItemProperties.ON_CLICK_KEY));
        }
    }
}
