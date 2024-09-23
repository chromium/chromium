// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Controller to manage desktop site settings in-product-help messages to users. */
public class DesktopSiteSettingsIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final WindowAndroid mWindowAndroid;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final ActivityTabProvider mActivityTabProvider;
    private final MessageDispatcher mMessageDispatcher;
    private final WebsitePreferenceBridge mWebsitePreferenceBridge;
    private ActivityTabTabObserver mActivityTabTabObserver;
    private final Context mContext;

    /**
     * Creates and initializes the controller. Registers an {@link ActivityTabTabObserver} that
     * attempts to show the following IPHs:
     * 1. The desktop site per-site settings IPH on an eligible tab on any site on a tablet device.
     * 2. The desktop site window setting IPH on a mobile site with an active window setting.
     *
     * @param activity The current activity.
     * @param windowAndroid The window associated with the activity.
     * @param activityTabProvider The provider of the current activity tab.
     * @param profile The current {@link Profile}.
     * @param toolbarMenuButton The toolbar menu button to which the IPH will be anchored.
     * @param appMenuHandler The app menu handler.
     */
    public static DesktopSiteSettingsIPHController create(
            Activity activity,
            WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler) {
        return new DesktopSiteSettingsIPHController(
                windowAndroid,
                activityTabProvider,
                profile,
                toolbarMenuButton,
                appMenuHandler,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())),
                new WebsitePreferenceBridge(),
                MessageDispatcherProvider.from(windowAndroid));
    }

    DesktopSiteSettingsIPHController(
            WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            UserEducationHelper userEducationHelper,
            WebsitePreferenceBridge websitePreferenceBridge,
            MessageDispatcher messageDispatcher) {
        mWindowAndroid = windowAndroid;
        mToolbarMenuButton = toolbarMenuButton;
        mContext = mToolbarMenuButton.getContext();
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = userEducationHelper;
        mActivityTabProvider = activityTabProvider;
        mWebsitePreferenceBridge = websitePreferenceBridge;
        mMessageDispatcher = messageDispatcher;

        createActivityTabTabObserver(profile);
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
        }
    }

    @VisibleForTesting
    void showGenericIPH(@NonNull Tab tab, Profile profile) {
        if (tab.isNativePage()) return;
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        String featureName = FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE;
        if (perSiteIPHPreChecksFailed(tab, tracker, featureName)) return;

        var siteExceptions =
                mWebsitePreferenceBridge.getContentSettingsExceptions(
                        profile, ContentSettingsType.REQUEST_DESKTOP_SITE);
        // Do not trigger the IPH if the user has already added any site-level exceptions. By
        // default, `siteExceptions` will hold one entry representing the wildcard for all sites,
        // for the default content setting.
        if (siteExceptions.size() > 1) {
            return;
        }

        boolean isTabUsingDesktopUserAgent =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        int textId =
                isTabUsingDesktopUserAgent
                        ? R.string.rds_site_settings_generic_iph_text_mobile
                        : R.string.rds_site_settings_generic_iph_text_desktop;

        requestShowPerSiteIPH(featureName, textId, new Object[] {tab.getUrl().getHost()});
    }

    ActivityTabTabObserver getActiveTabObserverForTesting() {
        return mActivityTabTabObserver;
    }

    @VisibleForTesting
    boolean perSiteIPHPreChecksFailed(@NonNull Tab tab, Tracker tracker, String featureName) {
        if (!DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) return true;

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

    @VisibleForTesting
    boolean showWindowSettingIPH(@NonNull Tab tab, Profile profile) {
        if (mMessageDispatcher == null) return false;
        if (tab.isNativePage()) return false;

        // Return early when the IPH triggering criteria is not satisfied.
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        String featureName = FeatureConstants.REQUEST_DESKTOP_SITE_WINDOW_SETTING_FEATURE;
        if (!tracker.wouldTriggerHelpUI(featureName)) return false;

        Resources resources = mContext.getResources();
        String titleText = resources.getString(R.string.rds_window_setting_message_title);
        String buttonText = resources.getString(R.string.rds_window_setting_message_button);

        // Check whether the site is currently using the desktop UA.
        boolean desktopUserAgentInUse =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        // Check whether desktop UA is globally enabled.
        boolean desktopSiteGloballyUsed =
                WebsitePreferenceBridge.isCategoryEnabled(
                        profile, ContentSettingsType.REQUEST_DESKTOP_SITE);
        // Check whether the site has a site-level exception.
        boolean siteExceptionExists =
                TabUtils.isDesktopSiteEnabled(profile, tab.getUrl()) != desktopSiteGloballyUsed;

        // Show the message only when the site uses the mobile UA without a desktop site exception,
        // and desktop site is globally enabled, indicating that the window setting is in use.
        if (desktopUserAgentInUse || siteExceptionExists || !desktopSiteGloballyUsed) {
            return false;
        }

        // Build and show the message.
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.DESKTOP_SITE_WINDOW_SETTING)
                        .with(MessageBannerProperties.TITLE, titleText)
                        .with(
                                MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_desktop_windows)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, buttonText)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    SiteSettingsHelper.showCategorySettings(
                                            mContext,
                                            SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .build();

        mMessageDispatcher.enqueueMessage(
                message, tab.getWebContents(), MessageScopeType.ORIGIN, false);
        TrackerFactory.getTrackerForProfile(profile)
                .notifyEvent(EventConstants.REQUEST_DESKTOP_SITE_WINDOW_SETTING_IPH_SHOWN);
        return true;
    }

    private void requestShowPerSiteIPH(String featureName, int textId, Object[] textArgs) {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mContext.getResources(),
                                featureName,
                                textId,
                                textArgs,
                                textId,
                                textArgs)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.request_desktop_site_id))
                        .setOnDismissCallback(
                                () -> {
                                    turnOffHighlightForMenuItem();
                                })
                        .build());
    }

    private void createActivityTabTabObserver(Profile profile) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        if (tab == null) return;
                        showGenericIPH(tab, profile);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        if (tab == null) return;
                        boolean windowSettingIphShown = showWindowSettingIPH(tab, profile);
                        if (!windowSettingIphShown) {
                            showGenericIPH(tab, profile);
                        }
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
