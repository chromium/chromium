// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import androidx.annotation.IdRes;

import org.chromium.base.Function;
import org.chromium.base.UnownedUserData;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * This class is responsible for managing the in-product help for the PWA app banners.
 */
public class AppBannerInProductHelpController implements UnownedUserData {
    private final Activity mActivity;
    private final AppMenuHandler mAppMenuHandler;
    private final Supplier<View> mMenuButtonView;
    private final Handler mHandler = new Handler();
    private final UserEducationHelper mUserEducationHelper;
    private final @IdRes int mHiglightMenuItemId;

    /**
     * Constructs an AppBannerInProductHelpController.
     * @param activity The current activity.
     * @param appMenuHandler The app menu containing the menu entry to highlight.
     * @param menuButtonView The menu button view to anchor the bubble to.
     * @param higlightMenuItemId The id of the menu item to highlight.
     * @param trackerFromProfileFactory The factory to obtain a tracker from.
     */
    public AppBannerInProductHelpController(Activity activity, AppMenuHandler appMenuHandler,
            Supplier<View> menuButtonView, @IdRes int higlightMenuItemId,
            Function<Profile, Tracker> trackerFromProfileFactory) {
        mActivity = activity;
        mAppMenuHandler = appMenuHandler;
        mMenuButtonView = menuButtonView;
        mHiglightMenuItemId = higlightMenuItemId;
        mUserEducationHelper =
                new UserEducationHelper(mActivity, mHandler, trackerFromProfileFactory);
    }

    /**
     * Makes an asynchronous request to show the in-product help, anchored to app menu.
     */
    public void requestInProductHelp() {
        IPHCommandBuilder builder = new IPHCommandBuilder(mActivity.getResources(),
                FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE,
                R.string.iph_pwa_install_available_text, R.string.iph_pwa_install_available_text);
        mUserEducationHelper.requestShowIPH(
                builder.setAnchorView(mMenuButtonView.get())
                        .setOnShowCallback(this::turnOnHighlightForMenu)
                        .setOnDismissCallback(this::turnOffHighlightForMenu)
                        .build());
    }

    private void turnOnHighlightForMenu() {
        mAppMenuHandler.setMenuHighlight(mHiglightMenuItemId);
    }

    private void turnOffHighlightForMenu() {
        mAppMenuHandler.clearMenuHighlight();
    }
}
