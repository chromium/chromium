// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Controller to manage PDF page in-product-help messages to users. */
@NullMarked
public class PdfPageIphController {
    private final UserEducationHelper mUserEducationHelper;
    private final WindowAndroid mWindowAndroid;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final ActivityTabProvider mActivityTabProvider;
    private ActivityTabTabObserver mActivityTabTabObserver;
    private final Context mContext;
    private final boolean mIsBrowserApp;

    /**
     * Creates and initializes the controller. Registers an {@link ActivityTabTabObserver} that
     * attempts to show the pdf download IPH.
     *
     * @param activity The current activity.
     * @param windowAndroid The window associated with the activity.
     * @param activityTabProvider The provider of the current activity tab.
     * @param profile The current {@link Profile}.
     * @param toolbarMenuButton The toolbar menu button to which the IPH will be anchored.
     * @param appMenuHandler The app menu handler.
     * @param isBrowserApp Whether the current activity is ChromeTabbedActivity.
     */
    public static @Nullable PdfPageIphController create(
            Activity activity,
            WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            boolean isBrowserApp) {
        if (!PdfUtils.shouldOpenPdfInline(profile.isOffTheRecord())) {
            return null;
        }
        return new PdfPageIphController(
                windowAndroid,
                activityTabProvider,
                profile,
                toolbarMenuButton,
                appMenuHandler,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())),
                isBrowserApp);
    }

    PdfPageIphController(
            WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            UserEducationHelper userEducationHelper,
            boolean isBrowserApp) {
        mWindowAndroid = windowAndroid;
        mToolbarMenuButton = toolbarMenuButton;
        mContext = mToolbarMenuButton.getContext();
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = userEducationHelper;
        mActivityTabProvider = activityTabProvider;
        mIsBrowserApp = isBrowserApp;

        createActivityTabTabObserver(profile);
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
        }
    }

    private void createActivityTabTabObserver(Profile profile) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        if (tab == null
                                || !tab.isNativePage()
                                || !assumeNonNull(tab.getNativePage()).isPdf()) {
                            return;
                        }
                        showDownloadIph(profile);
                    }
                };
    }

    ActivityTabTabObserver getActiveTabObserverForTesting() {
        return mActivityTabTabObserver;
    }

    private void showDownloadIph(Profile profile) {
        boolean isTablet = DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        String featureName = FeatureConstants.IPH_PDF_PAGE_DOWNLOAD;
        if (!tracker.wouldTriggerHelpUi(featureName)) {
            return;
        }

        int highlightMenuItemId;
        if (isTablet && mIsBrowserApp) {
            highlightMenuItemId = R.id.download_page_id;
        } else {
            highlightMenuItemId = R.id.offline_page_id;
        }
        requestShowDownloadIph(featureName, highlightMenuItemId);
    }

    private void requestShowDownloadIph(String featureName, int highlightMenuItemId) {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mContext.getResources(),
                                featureName,
                                R.string.pdf_page_download_iph_text,
                                R.string.pdf_page_download_iph_text)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(highlightMenuItemId))
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
