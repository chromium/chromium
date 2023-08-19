// Copyright 2021 The Chromium Authors
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
        } else if (FollowManagementItemProperties.URL_KEY == propertyKey) {
            view.setUrl(model.get(FollowManagementItemProperties.URL_KEY));
        } else if (FollowManagementItemProperties.STATUS_KEY == propertyKey) {
            view.setStatus(model.get(FollowManagementItemProperties.STATUS_KEY));
        } else if (FollowManagementItemProperties.ON_CLICK_KEY == propertyKey) {
            view.setCheckboxClickListener(model.get(FollowManagementItemProperties.ON_CLICK_KEY));
        } else if (FollowManagementItemProperties.SUBSCRIBED_KEY == propertyKey) {
            view.setSubscribed(model.get(FollowManagementItemProperties.SUBSCRIBED_KEY));
        } else if (FollowManagementItemProperties.CHECKBOX_ENABLED_KEY == propertyKey) {
            view.setCheckboxEnabled(model.get(FollowManagementItemProperties.CHECKBOX_ENABLED_KEY));
        } else if (FollowManagementItemProperties.FAVICON_KEY == propertyKey) {
            view.setFavicon(model.get(FollowManagementItemProperties.FAVICON_KEY));
        }
    }
}
