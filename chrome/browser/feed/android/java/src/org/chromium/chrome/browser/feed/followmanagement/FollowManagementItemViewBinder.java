// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class FollowManagementItemViewBinder {
    public static void bind(
            PropertyModel model, FollowManagementItemView view, PropertyKey propertyKey) {
        if (FollowManagementItemProperties.TITLE_KEY == propertyKey) {
            view.setTitle(model.get(FollowManagementItemProperties.TITLE_KEY));
        } else if (FollowManagementItemProperties.DESCRIPTION_KEY == propertyKey) {
            view.setDescription(model.get(FollowManagementItemProperties.DESCRIPTION_KEY));
        } else if (FollowManagementItemProperties.ON_CLICK_KEY == propertyKey) {
            view.setOnClickListener(model.get(FollowManagementItemProperties.ON_CLICK_KEY));
        }
        // TODO(petewil): Favicon, checkbox
    }
}
