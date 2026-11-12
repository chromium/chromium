// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller for In-Product Help for the Page Zoom feature. */
@NullMarked
public class PageZoomIphController {
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final UserEducationHelper mUserEducationHelper;

    public PageZoomIphController(
            Activity activity,
            Profile profile,
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton) {
        this(
                appMenuHandler,
                toolbarMenuButton,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())));
    }

    protected PageZoomIphController(
            AppMenuHandler appMenuHandler,
            View toolbarMenuButton,
            UserEducationHelper userEducationHelper) {
        mAppMenuHandler = appMenuHandler;
        mToolbarMenuButton = toolbarMenuButton;
        mUserEducationHelper = userEducationHelper;
    }

    /**
     * Returns whether the Page Zoom IPH may be shown while {@code tab} is the current tab. It is
     * only shown on non-NTP, non-native pages when the user has not already interacted with the
     * Page Zoom feature (no saved site zoom levels, no custom default zoom level, and no custom
     * AppMenu visibility preference in Accessibility Settings). The WebUI NTP is not a native page,
     * so it is excluded by URL.
     */
    public static boolean shouldShowForTab(@Nullable Tab tab) {
        // Page Zoom IPH should only show on non-NTP, non-Native pages.
        boolean isQualifyingPage =
                tab != null
                        && tab.getWebContents() != null
                        && !tab.isNativePage()
                        && !UrlUtilities.isNtpUrl(tab.getUrl());
        if (!isQualifyingPage) return false;

        // Page Zoom IPH should not show if the user has interacted with the feature in any way.
        Profile profile = assumeNonNull(tab).getProfile();
        boolean hasUsedPageZoomFeature =
                PageZoomUtils.hasAnySavedZoomLevels(profile)
                        || !PageZoomUtils.usesDefaultPageZoomLevel(profile)
                        || PageZoomUtils.hasUserSetShouldAlwaysShowZoomMenuItemOption();

        return !hasUsedPageZoomFeature;
    }

    /**
     * Make a request to the |UserEducationHelper| to show the In-Product Help text bubble for the
     * Page Zoom feature. When shown, the Zoom option in the main menu will be highlighted, and the
     * Tracker for the current Profile will be notified.
     */
    public void showColdStartIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mToolbarMenuButton.getContext().getResources(),
                                FeatureConstants.PAGE_ZOOM_FEATURE,
                                R.string.page_zoom_iph_message,
                                R.string.page_zoom_iph_message)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.page_zoom_id))
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
