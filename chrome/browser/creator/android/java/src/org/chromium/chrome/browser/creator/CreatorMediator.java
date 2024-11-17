// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;

import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Sets up the Mediator for Cormorant Creator surface. It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorMediator {
    private PropertyModel mCreatorModel;
    private final CreatorSnackbarController mCreatorSnackbarController;
    private SignInInterstitialInitiator mSignInInterstitialInitiator;

    CreatorMediator(
            Context context,
            PropertyModel creatorModel,
            CreatorSnackbarController creatorSnackbarController,
            SignInInterstitialInitiator signInInterstitialInitiator) {
        mCreatorModel = creatorModel;
        mCreatorSnackbarController = creatorSnackbarController;
        mSignInInterstitialInitiator = signInInterstitialInitiator;

        // Set Follow OnClick Action
        mCreatorModel.set(CreatorProperties.ON_FOLLOW_CLICK_KEY, this::followClickHandler);
        mCreatorModel.set(CreatorProperties.ON_FOLLOWING_CLICK_KEY, this::followingClickHandler);
    }

    private void followClickHandler() {
        if (FeedServiceBridge.isSignedIn()) {
            WebFeedBridge.followFromId(
                    mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY),
                    /* isDurable= */ false,
                    WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED,
                    (result) -> {
                        if (result.requestStatus == SUCCESS) {
                            mCreatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, true);
                        }
                        mCreatorSnackbarController.showSnackbarForFollow(
                                result.requestStatus,
                                mCreatorModel.get(CreatorProperties.TITLE_KEY));
                    });
        } else {
            mSignInInterstitialInitiator.showSignInInterstitial();
        }
    }

    private void followingClickHandler() {
        WebFeedBridge.unfollow(
                mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY),
                /* isDurable= */ false,
                WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED,
                (result) -> {
                    if (result.requestStatus == SUCCESS) {
                        mCreatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, false);
                    }
                    mCreatorSnackbarController.showSnackbarForUnfollow(
                            result.requestStatus, mCreatorModel.get(CreatorProperties.TITLE_KEY));
                });
    }
}
