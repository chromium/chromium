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
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.provider.Settings;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for the Incognito reauth lock settings in Privacy and security. TODO(crbug.com/40056462) :
 * Add reauth check when user toggles the setting once the reauth feature is implemented.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
public class IncognitoReauthSettingTest {
    private final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    private PrivacySettings mPrivacySettings;

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private void startSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        mPrivacySettings = mSettingsActivityTestRule.getFragment();
    }

    @Test
    @LargeTest
    public void testIncognitoReauthSetting_WhenDisabled_AndOnClickSummary() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        startSettings();
        Intent intent = new Intent();
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, intent);
        Intents.init();
        intending(anyIntent()).respondWith(result);
        String summaryText =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getString(
                                R.string.settings_incognito_tab_lock_summary_android_setting_off);
        summaryText = summaryText.replaceAll("</?link>", "");
        scrollToSetting(withText(summaryText));
        onView(withText(summaryText)).perform(click());
        intended(allOf(hasAction(Settings.ACTION_SECURITY_SETTINGS)));
        Intents.release();
    }
}
