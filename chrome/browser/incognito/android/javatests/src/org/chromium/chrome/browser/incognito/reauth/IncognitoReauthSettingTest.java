// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.provider.Settings;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.LargeTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for the Incognito reauth lock settings in Privacy and security.
 * TODO(crbug.com/1227656) : Add reauth check when user toggles the setting once the reauth feature
 * is implemented.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
public class IncognitoReauthSettingTest {
    private final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    private PrivacySettings mPrivacySettings;

    private void launchSettingsActivity() {
        mSettingsActivityTestRule.startSettingsActivity();
        mPrivacySettings = mSettingsActivityTestRule.getFragment();
    }

    @Test
    @LargeTest
    public void testIncognitoReauthSetting_WhenDisabled_AndOnClickSummary() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(false);
        launchSettingsActivity();
        Intent intent = new Intent();
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, intent);
        Intents.init();
        intending(anyIntent()).respondWith(result);
        String summaryText = ApplicationProvider.getApplicationContext().getResources().getString(
                R.string.settings_incognito_tab_lock_summary_android_setting_off);
        summaryText = summaryText.replaceAll("</?link>", "");
        onView(withText(summaryText)).perform(click());
        intended(allOf(hasAction(Settings.ACTION_SECURITY_SETTINGS)));
        Intents.release();
    }
}
