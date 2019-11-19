// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;

/**
 * Util functions for testing SiteSettings functionality.
 */
public class SiteSettingsTestUtils {
    public static Preferences startSiteSettingsMenu(String category) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategoryPreferences.EXTRA_CATEGORY, category);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                InstrumentationRegistry.getTargetContext(), SiteSettingsPreferences.class.getName(),
                fragmentArgs);
        return (Preferences) InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static Preferences startSiteSettingsCategory(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                SingleCategoryPreferences.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                InstrumentationRegistry.getTargetContext(),
                SingleCategoryPreferences.class.getName(), fragmentArgs);
        return (Preferences) InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    public static Preferences startSingleWebsitePreferences(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsitePreferences.EXTRA_SITE, site);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                InstrumentationRegistry.getTargetContext(),
                SingleWebsitePreferences.class.getName(), fragmentArgs);
        return (Preferences) InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }
}
