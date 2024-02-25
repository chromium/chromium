// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder class for the Creator Profile section */
public class CreatorProfileViewBinder {
    public static void bind(PropertyModel model, CreatorProfileView view, PropertyKey propertyKey) {
        if (CreatorProperties.TITLE_KEY == propertyKey) {
            view.setTitle(model.get(CreatorProperties.TITLE_KEY));
        } else if (CreatorProperties.FORMATTED_URL_KEY == propertyKey) {
            view.setUrl(model.get(CreatorProperties.FORMATTED_URL_KEY));
        } else if (CreatorProperties.IS_TOOLBAR_VISIBLE_KEY == propertyKey) {
            view.setProfileVisibility(model.get(CreatorProperties.IS_TOOLBAR_VISIBLE_KEY));
        } else if (CreatorProperties.IS_FOLLOWED_KEY == propertyKey) {
            view.setIsFollowedStatus(model.get(CreatorProperties.IS_FOLLOWED_KEY));
        } else if (CreatorProperties.ON_FOLLOW_CLICK_KEY == propertyKey) {
            view.setFollowButtonOnClickListener(model.get(CreatorProperties.ON_FOLLOW_CLICK_KEY));
        } else if (CreatorProperties.ON_FOLLOWING_CLICK_KEY == propertyKey) {
            view.setFollowingButtonOnClickListener(
                    model.get(CreatorProperties.ON_FOLLOWING_CLICK_KEY));
        }
    }
}
