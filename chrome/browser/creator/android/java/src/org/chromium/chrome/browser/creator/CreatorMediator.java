// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;

import org.chromium.chrome.browser.creator.CreatorApiBridge.Creator;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Sets up the Mediator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorMediator {
    private Context mContext;
    private Creator mCreator;
    private String mTitle;
    private String mUrl;
    private PropertyModel mCreatorProfileModel;
    private boolean mFollowState;

    CreatorMediator(Context context, PropertyModel creatorProfileModel) {
        mContext = context;
        mCreatorProfileModel = creatorProfileModel;

        // Set Follow OnClick Action
        mCreatorProfileModel.set(
                CreatorProfileProperties.ON_FOLLOW_CLICK_KEY, this::followClickHandler);
        mCreatorProfileModel.set(
                CreatorProfileProperties.ON_FOLLOWING_CLICK_KEY, this::followingClickHandler);

        // TODO(crbug.com/1377071): Set up Title and URL dynamically using CreatorBridge
    }

    private void followClickHandler() {
        WebFeedBridge.followFromId(
                mCreatorProfileModel.get(CreatorProfileProperties.WEB_FEED_ID_KEY),
                /*isDurable=*/false, WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU, (result) -> {
                    if (result.requestStatus == SUCCESS) {
                        mCreatorProfileModel.set(CreatorProfileProperties.IS_FOLLOWED_KEY, true);
                    }
                });
    }

    private void followingClickHandler() {
        WebFeedBridge.unfollow(mCreatorProfileModel.get(CreatorProfileProperties.WEB_FEED_ID_KEY),
                /*isDurable=*/false, WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU, (result) -> {
                    if (result.requestStatus == SUCCESS) {
                        mCreatorProfileModel.set(CreatorProfileProperties.IS_FOLLOWED_KEY, false);
                    }
                });
    }

    private void getCreator() {
        CreatorApiBridge.getCreator("test", this::onGetCreator);
    }

    private void onGetCreator(Creator creator) {
        mCreator = creator;
    }
}
