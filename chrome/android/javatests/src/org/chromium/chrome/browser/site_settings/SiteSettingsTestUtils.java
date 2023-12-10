// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
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
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Util functions for testing SiteSettings functionality. */
public class SiteSettingsTestUtils {
    public static SettingsActivity startSiteSettingsMenu(String category) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategorySettings.EXTRA_CATEGORY, category);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(
                        ApplicationProvider.getApplicationContext(),
                        SiteSettings.class.getName(),
                        fragmentArgs);
        return (SettingsActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static SettingsActivity startSiteSettingsCategory(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                SingleCategorySettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        String title =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            Context context =
                                    InstrumentationRegistry.getInstrumentation().getContext();
                            var delegate =
                                    new ChromeSiteSettingsDelegate(
                                            context, Profile.getLastUsedRegularProfile());
                            return context.getResources()
                                    .getString(
                                            ContentSettingsResources.getTitleForCategory(
                                                    type, delegate));
                        });
        fragmentArgs.putString(SingleCategorySettings.EXTRA_TITLE, title);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(
                        ApplicationProvider.getApplicationContext(),
                        SingleCategorySettings.class.getName(),
                        fragmentArgs);
        return (SettingsActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static SettingsActivity startStorageAccessSettingsActivity(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(StorageAccessSubpageSettings.EXTRA_STORAGE_ACCESS_STATE, site);
        fragmentArgs.putBoolean(StorageAccessSubpageSettings.EXTRA_ALLOWED, true);

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(
                        context, StorageAccessSubpageSettings.class.getName(), fragmentArgs);
        return (SettingsActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static SettingsActivity startSingleWebsitePreferences(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsiteSettings.EXTRA_SITE, site);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(
                        ApplicationProvider.getApplicationContext(),
                        SingleWebsiteSettings.class.getName(),
                        fragmentArgs);
        return (SettingsActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static SettingsActivity startGroupedWebsitesPreferences(WebsiteGroup group) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(GroupedWebsitesSettings.EXTRA_GROUP, group);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(
                        ApplicationProvider.getApplicationContext(),
                        GroupedWebsitesSettings.class.getName(),
                        fragmentArgs);
        return (SettingsActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static SettingsActivity startAllSitesSettings(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                AllSiteSettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(
                        ApplicationProvider.getApplicationContext(),
                        AllSiteSettings.class.getName(),
                        fragmentArgs);
        return (SettingsActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static RadioButtonWithDescriptionAndAuxButton getCookieRadioButtonFrom(
            TriStateCookieSettingsPreference cookiePage,
            @CookieControlsMode int cookieControlsMode) {
        RadioButtonWithDescription button = cookiePage.getButton(cookieControlsMode);

        return ((RadioButtonWithDescriptionAndAuxButton) button);
    }
}
