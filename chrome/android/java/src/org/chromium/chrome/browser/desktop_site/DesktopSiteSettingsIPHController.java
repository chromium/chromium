// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Controller to manage desktop site settings in-product-help messages to users.
 */
public class DesktopSiteSettingsIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final WindowAndroid mWindowAndroid;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final ActivityTabProvider mActivityTabProvider;
    private final WebsitePreferenceBridge mWebsitePreferenceBridge;
    private ActivityTabTabObserver mActivityTabTabObserver;

    /**
     * Creates and initializes the controller.
     * Registers an {@link ActivityTabTabObserver} that will attempt to show the desktop site
     * per-site settings IPH on an eligible tab in one of the following cases:
     * 1. On a specific site included in a pre-defined list (arm 1).
     * 2. On any site on a tablet device (arm 2).
     *
     * @param activity The current activity.
     * @param windowAndroid The window associated with the activity.
     * @param activityTabProvider The provider of the current activity tab.
     * @param profile The current {@link Profile}.
     * @param toolbarMenuButton The toolbar menu button to which the IPH will be anchored.
     * @param appMenuHandler The app menu handler.
     */
    public static DesktopSiteSettingsIPHController create(Activity activity,
            WindowAndroid windowAndroid, ActivityTabProvider activityTabProvider, Profile profile,
            View toolbarMenuButton, AppMenuHandler appMenuHandler) {
        return new DesktopSiteSettingsIPHController(windowAndroid, activityTabProvider, profile,
                toolbarMenuButton, appMenuHandler,
                new UserEducationHelper(activity, new Handler(Looper.getMainLooper())),
                new WebsitePreferenceBridge());
    }

    DesktopSiteSettingsIPHController(WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider, Profile profile, View toolbarMenuButton,
            AppMenuHandler appMenuHandler, UserEducationHelper userEducationHelper,
            WebsitePreferenceBridge websitePreferenceBridge) {
        mWindowAndroid = windowAndroid;
        mToolbarMenuButton = toolbarMenuButton;
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = userEducationHelper;
        mActivityTabProvider = activityTabProvider;
        mWebsitePreferenceBridge = websitePreferenceBridge;

        maybeCreateTabObserverForPerSiteIPH(profile);
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
        }
    }

    @VisibleForTesting
    void showGenericIPH(@NonNull Tab tab, Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        String featureName = FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE;
        if (perSiteIPHPreChecksFailed(tab, tracker, featureName)) return;

        var siteExceptions = mWebsitePreferenceBridge.getContentSettingsExceptions(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE);
        // Do not trigger the IPH if the user has already added any site-level exceptions. By
        // default, `siteExceptions` will hold one entry representing the wildcard for all sites,
        // for the default content setting.
        if (siteExceptions.size() > 1) {
            return;
        }

        boolean isTabUsingDesktopUserAgent =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        int textId = isTabUsingDesktopUserAgent
                ? R.string.rds_site_settings_generic_iph_text_mobile
                : R.string.rds_site_settings_generic_iph_text_desktop;

        requestShowPerSiteIPH(featureName, textId, new Object[] {tab.getUrl().getHost()});
    }

    ActivityTabTabObserver getActiveTabObserverForTesting() {
        return mActivityTabTabObserver;
    }

    // Run pre-checks common to both per-site settings IPHs.
    @VisibleForTesting
    boolean perSiteIPHPreChecksFailed(@NonNull Tab tab, Tracker tracker, String featureName) {
        // Return early when the IPH triggering criteria is not satisfied.
        if (!tracker.wouldTriggerHelpUI(featureName)) {
            return true;
        }

        // Do not trigger the IPH on an incognito tab since the setting does not persist.
        if (tab.isIncognito()) {
            return true;
        }

        GURL url = tab.getUrl();
        // Do not trigger the IPH on a chrome:// or a chrome-native:// page.
        return UrlUtilities.isInternalScheme(url) || tab.getWebContents() == null;
    }

    private void requestShowPerSiteIPH(String featureName, int textId, Object[] textArgs) {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mToolbarMenuButton.getContext().getResources(), featureName,
                        textId, textArgs, textId, textArgs)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.request_desktop_site_id))
                        .setOnDismissCallback(() -> {
                            turnOffHighlightForMenuItem();
                            RecordHistogram.recordBooleanHistogram(
                                    "Android.RequestDesktopSite.PerSiteIphDismissed.AppMenuOpened",
                                    mAppMenuHandler.isAppMenuShowing());
                        })
                        .build());
    }

    private void maybeCreateTabObserverForPerSiteIPH(Profile profile) {
        if (!DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) {
            return;
        }
        createActivityTabTabObserver(tab -> showGenericIPH(tab, profile));
    }

    private void createActivityTabTabObserver(Callback<Tab> showIPHCallback) {
        mActivityTabTabObserver = new ActivityTabTabObserver(mActivityTabProvider) {
            @Override
            protected void onObservingDifferentTab(Tab tab, boolean hint) {
                if (tab == null) return;
                showIPHCallback.onResult(tab);
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                if (tab == null) return;
                showIPHCallback.onResult(tab);
            }
        };
    }

    private void turnOnHighlightForMenuItem(int highlightMenuItemId) {
        mAppMenuHandler.setMenuHighlight(highlightMenuItemId);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }
}
