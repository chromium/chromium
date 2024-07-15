// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Locale;

/** Integration tests for IncognitoNewTabPage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures({ChromeFeatureList.TRACKING_PROTECTION_3PCD})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNewTabPageTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private void setCookieControlsMode(@CookieControlsMode int mode) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setInteger(PrefNames.COOKIE_CONTROLS_MODE, mode);
                });
    }

    private void assertCookieControlsMode(@CookieControlsMode int mode) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getInteger(PrefNames.COOKIE_CONTROLS_MODE),
                            mode);
                });
    }

    private void enableTrackingProtection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED, true);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());

                    prefService.clearPref(Pref.TRACKING_PROTECTION3PCD_ENABLED);
                    prefService.clearPref(PrefNames.COOKIE_CONTROLS_MODE);
                });
    }

    /** Test cookie controls toggle defaults to on if cookie controls mode is on. */
    @Test
    @SmallTest
    public void testCookieControlsToggleStartsOn() throws Exception {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        sActivityTestRule.newIncognitoTabFromMenu();

        // Make sure cookie controls card is visible
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        // Assert the cookie controls toggle is checked
        onView(withId(R.id.cookie_controls_card_toggle)).check(matches(isChecked()));
    }

    /** Test cookie controls toggle turns on and off cookie controls mode as expected. */
    @Test
    @SmallTest
    public void testCookieControlsToggleChanges() throws Exception {
        setCookieControlsMode(CookieControlsMode.OFF);
        sActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        int toggle_id = R.id.cookie_controls_card_toggle;
        // Toggle should start unchecked
        onView(withId(toggle_id)).check(matches(isNotChecked()));
        // Toggle should be checked after click
        onView(withId(toggle_id)).perform(scrollTo(), click()).check(matches(isChecked()));
        // CookieControlsMode should be incognito_only
        assertCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        // Toggle should be unchecked again after click
        onView(withId(toggle_id)).perform(scrollTo(), click()).check(matches(isNotChecked()));
        // CookieControlsMode should be off
        assertCookieControlsMode(CookieControlsMode.OFF);
    }

    /** Test cookie controls disabled if managed by settings. */
    @Test
    @SmallTest
    public void testCookieControlsToggleManaged() throws Exception {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        sActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        int toggle_id = R.id.cookie_controls_card_toggle;
        // Toggle should start checked and enabled
        onView(withId(toggle_id)).check(matches(allOf(isChecked(), isEnabled())));
        // Toggle should be disabled if managed by setting
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        onView(withId(toggle_id)).check(matches(not(isEnabled())));
        // Toggle should be enabled and remain checked
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        onView(withId(toggle_id)).check(matches(allOf(isChecked(), isEnabled())));

        // Repeat of above but toggle should remain unchecked
        onView(withId(toggle_id)).perform(scrollTo(), click());
        onView(withId(toggle_id)).check(matches(allOf(isNotChecked(), isEnabled())));
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        onView(withId(toggle_id)).check(matches(not(isEnabled())));
        setCookieControlsMode(CookieControlsMode.OFF);
        onView(withId(toggle_id)).check(matches(allOf(isNotChecked(), isEnabled())));
    }

    /** Test the tracking protection layout. */
    @Test
    @SmallTest
    public void testTrackingProtection() throws Exception {
        enableTrackingProtection();
        sActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.tracking_protection_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
    }

    private Context createContextForLocale(Context context, String languageTag) {
        Locale locale = LocaleUtils.forLanguageTag(languageTag);
        Resources res = context.getResources();
        Configuration config = res.getConfiguration();
        config.setLocale(locale);
        return context.createConfigurationContext(config);
    }

    /** Test the ntp text formatting for all locales. */
    @Test
    @SmallTest
    public void testDescriptionLanguages() throws Exception {
        var context = sActivityTestRule.getActivity().getApplicationContext();
        for (String languageTag : ProductConfig.LOCALES) {
            var localeContext = createContextForLocale(context, languageTag);
            IncognitoDescriptionView.getSpannedBulletText(
                    localeContext, R.string.new_tab_otr_not_saved);
            IncognitoDescriptionView.getSpannedBulletText(
                    localeContext, R.string.new_tab_otr_visible);
        }
    }
}
