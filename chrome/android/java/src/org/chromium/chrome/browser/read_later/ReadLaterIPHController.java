// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller to manage when and how we show read later in-product-help messages to users. */
public class ReadLaterIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;

    /**
     * Constructor.
     *
     * @param activity The current activity.
     * @param profile The current Profile.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler
     */
    public ReadLaterIPHController(
            Activity activity,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler) {
        this(
                activity,
                toolbarMenuButton,
                appMenuHandler,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())));
    }

    ReadLaterIPHController(
            Activity activity,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            UserEducationHelper userEducationHelper) {
        mToolbarMenuButton = toolbarMenuButton;
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = userEducationHelper;
    }

    /** Attempts to show an IPH text bubble about the read later option in app menu. */
    public void onCopyContextMenuItemClicked() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mToolbarMenuButton.getContext().getResources(),
                                FeatureConstants.READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE,
                                R.string.reading_list_save_pages_for_later,
                                R.string.reading_list_save_pages_for_later)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.bookmark_this_page_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /** Attempts to show an IPH text bubble after a cold start. */
    public void showColdStartIPH() {
        showReadLaterAppMenuBookmarksIPH();
    }

    private void showReadLaterAppMenuBookmarksIPH() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mToolbarMenuButton.getContext().getResources(),
                                FeatureConstants.READ_LATER_APP_MENU_BOOKMARKS_FEATURE,
                                R.string.reading_list_find_in_bookmarks,
                                R.string.reading_list_find_in_bookmarks)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.all_bookmarks_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    private void turnOnHighlightForMenuItem(int highlightMenuItemId) {
        mAppMenuHandler.setMenuHighlight(highlightMenuItemId);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }
}
