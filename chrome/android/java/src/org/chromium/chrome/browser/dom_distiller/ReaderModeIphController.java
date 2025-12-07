// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller for showing the reader mode in-product help (IPH). */
@NullMarked
public class ReaderModeIphController implements ReaderModeActionRateLimiter.Observer {
    private final UserEducationHelper mUserEducationHelper;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;

    /**
     * @param activity The current activity.
     * @param profile The current Profile.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler
     */
    public ReaderModeIphController(
            Activity activity,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler) {
        this(
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())),
                toolbarMenuButton,
                appMenuHandler);
    }

    ReaderModeIphController(
            UserEducationHelper userEducationHelper,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler) {
        mUserEducationHelper = userEducationHelper;
        mAppMenuHandler = appMenuHandler;
        mToolbarMenuButton = toolbarMenuButton;
    }

    /** Shows the reader mode IPH. */
    public void showIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mToolbarMenuButton.getResources(),
                                FeatureConstants.READER_MODE_DISTILL_IN_APP_FEATURE,
                                R.string.reader_mode_menu_iph,
                                R.string.reader_mode_menu_iph)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(
                                () -> mAppMenuHandler.setMenuHighlight(R.id.reader_mode_menu_id))
                        .setOnDismissCallback(mAppMenuHandler::clearMenuHighlight)
                        .build());
    }
}
