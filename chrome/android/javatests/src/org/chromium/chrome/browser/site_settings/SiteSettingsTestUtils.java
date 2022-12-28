// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference.CookieSettingsState;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Util functions for testing SiteSettings functionality.
 */
public class SiteSettingsTestUtils {
    public static SettingsActivity startSiteSettingsMenu(String category) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategorySettings.EXTRA_CATEGORY, category);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getTargetContext(), SiteSettings.class.getName(),
                fragmentArgs);
        return (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                intent);
    }

    public static SettingsActivity startSiteSettingsCategory(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                SingleCategorySettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        String title = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Context context = InstrumentationRegistry.getInstrumentation().getContext();
            var delegate =
                    new ChromeSiteSettingsDelegate(context, Profile.getLastUsedRegularProfile());
            return context.getResources().getString(
                    ContentSettingsResources.getTitleForCategory(type, delegate));
        });
        fragmentArgs.putString(SingleCategorySettings.EXTRA_TITLE, title);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getTargetContext(), SingleCategorySettings.class.getName(),
                fragmentArgs);
        return (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                intent);
    }

    public static SettingsActivity startSingleWebsitePreferences(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsiteSettings.EXTRA_SITE, site);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getTargetContext(), SingleWebsiteSettings.class.getName(),
                fragmentArgs);
        return (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                intent);
    }

    public static SettingsActivity startGroupedWebsitesPreferences(WebsiteGroup group) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(GroupedWebsitesSettings.EXTRA_GROUP, group);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getTargetContext(), GroupedWebsitesSettings.class.getName(),
                fragmentArgs);
        return (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                intent);
    }

    public static SettingsActivity startAllSitesSettings(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                AllSiteSettings.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getTargetContext(), AllSiteSettings.class.getName(),
                fragmentArgs);
        return (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                intent);
    }

    public static RadioButtonWithDescriptionAndAuxButton getCookieRadioButtonFrom(
            FourStateCookieSettingsPreference cookiePage, CookieSettingsState cookieSettingsState) {
        RadioButtonWithDescription button = cookiePage.getButton(cookieSettingsState);

        return ((RadioButtonWithDescriptionAndAuxButton) button);
    }
}
