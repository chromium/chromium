// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import androidx.annotation.IdRes;

import org.chromium.base.UnownedUserData;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * This class is responsible for managing the in-product help for the PWA app banners.
 */
public class AppBannerInProductHelpController implements UnownedUserData {
    private final Activity mActivity;
    private final Handler mHandler = new Handler();
    private final UserEducationHelper mUserEducationHelper;
    private final @IdRes int mHiglightMenuItemId;

    /**
     * Constructs an AppBannerInProductHelpController.
     * @param activity The current activity.
     * @param higlightMenuItemId The id of the menu item to highlight.
     */
    public AppBannerInProductHelpController(Activity activity,
            @IdRes int higlightMenuItemId) {
        mActivity = activity;
        mHiglightMenuItemId = higlightMenuItemId;
        mUserEducationHelper = new UserEducationHelper(mActivity, mHandler);
    }

    /**
     * Makes an asynchronous request to show the in-product help, anchored to app menu.
     */
    public void requestInProductHelp() {
    }

    private void turnOnHighlightForMenu() {
    }

    private void turnOffHighlightForMenu() {
    }
}
