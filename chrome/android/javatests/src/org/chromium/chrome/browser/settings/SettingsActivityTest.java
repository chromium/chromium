// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the Settings menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SettingsActivityTest {
    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @Test
    @SmallTest
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false")
        , @Policies.Item(key = "BrowserSignin", string = "0")
    })
    @DisableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING})
    public void
    testPasswordSettings_ManagedAndDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });

        CriteriaHelper.pollUiThread(() -> {
            return UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
        });

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.password_settings_title)).perform(click());
        onView(withText(R.string.password_settings_save_passwords)).check(matches(isDisplayed()));
    }
}
