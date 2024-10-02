// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.StorageAccessSubpageSettings;
import org.chromium.components.browser_ui.site_settings.TriStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.content_settings.CookieControlsMode;

import java.util.concurrent.TimeoutException;

/** Util functions for testing SiteSettings functionality. */
public class SiteSettingsTestUtils {
    public static SettingsActivity startSiteSettingsMenu(String category) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategorySettings.EXTRA_CATEGORY, category);
        return startSiteSettings(SiteSettings.class, fragmentArgs);
    }

    public static SettingsActivity startSiteSettingsCategory(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                SingleCategorySettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        String title =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Context context =
                                    InstrumentationRegistry.getInstrumentation().getContext();
                            return context.getResources()
                                    .getString(ContentSettingsResources.getTitleForCategory(type));
                        });
        fragmentArgs.putString(SingleCategorySettings.EXTRA_TITLE, title);
        return startSiteSettings(SingleCategorySettings.class, fragmentArgs);
    }

    public static SettingsActivity startStorageAccessSettingsActivity(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(StorageAccessSubpageSettings.EXTRA_STORAGE_ACCESS_STATE, site);
        fragmentArgs.putBoolean(StorageAccessSubpageSettings.EXTRA_ALLOWED, true);
        return startSiteSettings(StorageAccessSubpageSettings.class, fragmentArgs);
    }

    public static SettingsActivity startSingleWebsitePreferences(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsiteSettings.EXTRA_SITE, site);
        return startSiteSettings(SingleWebsiteSettings.class, fragmentArgs);
    }

    public static SettingsActivity startGroupedWebsitesPreferences(WebsiteGroup group) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(GroupedWebsitesSettings.EXTRA_GROUP, group);
        return startSiteSettings(GroupedWebsitesSettings.class, fragmentArgs);
    }

    public static SettingsActivity startAllSitesSettings(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                AllSiteSettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        return startSiteSettings(AllSiteSettings.class, fragmentArgs);
    }

    public static SettingsActivity startAllSitesSettingsForRws(
            @SiteSettingsCategory.Type int type, String rwsPage) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                AllSiteSettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        fragmentArgs.putString(AllSiteSettings.EXTRA_SEARCH, rwsPage);
        return startSiteSettings(AllSiteSettings.class, fragmentArgs);
    }

    private static SettingsActivity startSiteSettings(
            Class<? extends Fragment> fragmentClass, Bundle fragmentArgs) {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        ApplicationProvider.getApplicationContext(), fragmentClass, fragmentArgs);
        SettingsActivity settingsActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        SettingsActivity.class,
                        Stage.CREATED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));
        ApplicationTestUtils.waitForActivityState(settingsActivity, Stage.RESUMED);
        return settingsActivity;
    }

    public static RadioButtonWithDescriptionAndAuxButton getCookieRadioButtonFrom(
            TriStateCookieSettingsPreference cookiePage,
            @CookieControlsMode int cookieControlsMode) {
        RadioButtonWithDescription button = cookiePage.getButton(cookieControlsMode);

        return ((RadioButtonWithDescriptionAndAuxButton) button);
    }

    public static void cleanUpCookiesAndPermissions() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {
                                        BrowsingDataType.SITE_DATA, BrowsingDataType.SITE_SETTINGS
                                    },
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }
}
