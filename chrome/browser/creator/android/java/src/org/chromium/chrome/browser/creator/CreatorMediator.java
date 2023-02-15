// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;

import org.chromium.chrome.browser.creator.CreatorApiBridge.Creator;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.ui.modelutil.PropertyModel;

import java.nio.charset.StandardCharsets;

/**
 * Sets up the Mediator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorMediator {
    private Context mContext;
    private Creator mCreator;
    private PropertyModel mCreatorModel;
    private final CreatorSnackbarController mCreatorSnackbarController;

    CreatorMediator(Context context, PropertyModel creatorModel,
            CreatorSnackbarController creatorSnackbarController) {
        mContext = context;
        mCreatorModel = creatorModel;
        mCreatorSnackbarController = creatorSnackbarController;
        if (mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY) != null) {
            getCreator();
        }

        // Set Follow OnClick Action
        mCreatorModel.set(CreatorProperties.ON_FOLLOW_CLICK_KEY, this::followClickHandler);
        mCreatorModel.set(CreatorProperties.ON_FOLLOWING_CLICK_KEY, this::followingClickHandler);
    }

    private void followClickHandler() {
        WebFeedBridge.followFromId(mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY),
                /*isDurable=*/false, WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU, (result) -> {
                    if (result.requestStatus == SUCCESS) {
                        mCreatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, true);
                    }
                    mCreatorSnackbarController.showSnackbarForFollow(
                            result.requestStatus, mCreatorModel.get(CreatorProperties.TITLE_KEY));
                });
    }

    private void followingClickHandler() {
        WebFeedBridge.unfollow(mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY),
                /*isDurable=*/false, WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU, (result) -> {
                    if (result.requestStatus == SUCCESS) {
                        mCreatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, false);
                    }
                    mCreatorSnackbarController.showSnackbarForUnfollow(
                            result.requestStatus, mCreatorModel.get(CreatorProperties.TITLE_KEY));
                });
    }

    private void getCreator() {
        CreatorApiBridge.getCreator(new String(mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY),
                                            StandardCharsets.UTF_8),
                this::onGetCreator);
    }

    private void onGetCreator(Creator creator) {
        // TODO(crbug/1374058): Get Title and Url from CreatorAPI
        mCreator = creator;
    }
}
