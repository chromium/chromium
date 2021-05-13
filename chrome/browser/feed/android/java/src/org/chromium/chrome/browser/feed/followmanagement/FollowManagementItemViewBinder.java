// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.view.View;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class FollowManagementItemViewBinder {
    private static final String TAG = "FMItemViewBinder";
    public static void bind(
            PropertyModel model, FollowManagementItemView view, PropertyKey propertyKey) {
        if (FollowManagementItemProperties.TITLE_KEY == propertyKey) {
            view.setTitle(model.get(FollowManagementItemProperties.TITLE_KEY));
        } else if (FollowManagementItemProperties.URL_KEY == propertyKey) {
            view.setUrl(model.get(FollowManagementItemProperties.URL_KEY));
        } else if (FollowManagementItemProperties.STATUS_KEY == propertyKey) {
            view.setStatus(model.get(FollowManagementItemProperties.STATUS_KEY));
        } else if (FollowManagementItemProperties.ON_CLICK_KEY == propertyKey) {
            // Set the click handler on the checkbox, and only the checkbox.  We will assume
            // in the mediator that the view is a checkbox when the click event arrives.
            View checkbox = view.findViewById(R.id.follow_management_subscribed_checkbox);
            if (checkbox != null) {
                checkbox.setOnClickListener(model.get(FollowManagementItemProperties.ON_CLICK_KEY));
            } else {
                Log.d(TAG, "XML item layout did not have the checkbox we expected.");
            }
        } else if (FollowManagementItemProperties.SUBSCRIBED_KEY == propertyKey) {
            view.setSubscribed(
                    model.get(FollowManagementItemProperties.SUBSCRIBED_KEY).booleanValue());
        } else if (FollowManagementItemProperties.FAVICON_KEY == propertyKey) {
            view.setFavicon(model.get(FollowManagementItemProperties.FAVICON_KEY));
        }
    }
}
