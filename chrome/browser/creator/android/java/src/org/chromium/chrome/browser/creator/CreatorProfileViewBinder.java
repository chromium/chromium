// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder class for the Creator Profile section
 */
public class CreatorProfileViewBinder {
    public static void bind(PropertyModel model, CreatorProfileView view, PropertyKey propertyKey) {
        if (CreatorProfileProperties.TITLE_KEY == propertyKey) {
            view.setTitle(model.get(CreatorProfileProperties.TITLE_KEY));
        } else if (CreatorProfileProperties.URL_KEY == propertyKey) {
            view.setUrl(model.get(CreatorProfileProperties.URL_KEY));
        } else if (CreatorProfileProperties.IS_FOLLOWED_KEY == propertyKey) {
            view.setIsFollowedStatus(model.get(CreatorProfileProperties.IS_FOLLOWED_KEY));
        } else if (CreatorProfileProperties.ON_FOLLOW_CLICK_KEY == propertyKey) {
            view.setFollowButtonOnClickListener(
                    model.get(CreatorProfileProperties.ON_FOLLOW_CLICK_KEY));
        } else if (CreatorProfileProperties.ON_FOLLOWING_CLICK_KEY == propertyKey) {
            view.setFollowingButtonOnClickListener(
                    model.get(CreatorProfileProperties.ON_FOLLOWING_CLICK_KEY));
        }
    }
}
