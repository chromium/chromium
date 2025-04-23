// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.privacy_sandbox.IncognitoTrackingProtectionsFragment;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.Locale;

/** Integration tests for IncognitoNewTabPage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
    ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
})
@DisableFeatures({ChromeFeatureList.TRACKING_PROTECTION_3PCD})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNewTabPageTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private SettingsNavigation mSettingsNavigation;

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

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    /** Test cookie controls toggle defaults to on if cookie controls mode is on. */
    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
        ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
    })
    public void testCookieControlsToggleStartsOn() throws Exception {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        mActivityTestRule.newIncognitoTabFromMenu();

        // Make sure cookie controls card is visible
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        // Assert the cookie controls toggle is checked
        onView(withId(R.id.cookie_controls_card_toggle)).check(matches(isChecked()));
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    /** Test cookie controls toggle turns on and off cookie controls mode as expected. */
    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
        ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
    })
    public void testCookieControlsToggleChanges() throws Exception {
        setCookieControlsMode(CookieControlsMode.OFF);
        mActivityTestRule.newIncognitoTabFromMenu();
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

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    /** Test cookie controls disabled if managed by settings. */
    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO,
        ChromeFeatureList.FINGERPRINTING_PROTECTION_UX
    })
    public void testCookieControlsToggleManaged() throws Exception {
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        mActivityTestRule.newIncognitoTabFromMenu();
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
        mActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.tracking_protection_card))
                .perform(scrollTo())
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
    }

    // TODO(crbug.com/408036586): Remove once FingerprintingProtectionUx launched.
    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.FINGERPRINTING_PROTECTION_UX})
    public void incognitoNtpShowsThirdPartyCookieBlockingHeader() throws Exception {
        mActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.tracking_protection_card))
                .perform(scrollTo())
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        String ntpDescription =
                getIncognitoNtpDescriptionSpanned(
                        mActivityTestRule
                                .getActivity()
                                .getResources()
                                .getString(
                                        R.string
                                                .incognito_ntp_block_third_party_cookies_description_android));

        onView(withText(R.string.incognito_ntp_block_third_party_cookies_header))
                .check(matches(isDisplayed()));
        onView(withText(ntpDescription)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void incognitoNtpShowsIncongitoTrackingProtectionsHeader() throws Exception {
        mActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.tracking_protection_card))
                .perform(scrollTo())
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        String ntpDescription =
                getIncognitoNtpDescriptionSpanned(
                        mActivityTestRule
                                .getActivity()
                                .getResources()
                                .getString(
                                        R.string
                                                .incognito_ntp_incognito_tracking_protections_description_android));

        onView(withText(R.string.incognito_ntp_incognito_tracking_protections_header))
                .check(matches(isDisplayed()));
        onView(withText(ntpDescription)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void incognitoTrackingProtectionsLinkNavigatesToItpSettings() throws Exception {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mActivityTestRule.newIncognitoTabFromMenu();

        String ntpDescription =
                getIncognitoNtpDescriptionSpanned(
                        mActivityTestRule
                                .getActivity()
                                .getResources()
                                .getString(
                                        R.string
                                                .incognito_ntp_incognito_tracking_protections_description_android));
        onView(withText(ntpDescription)).perform(clickOnClickableSpan(0));

        Mockito.verify(mSettingsNavigation)
                .startSettings(any(), eq(IncognitoTrackingProtectionsFragment.class));
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
        var context = mActivityTestRule.getActivity().getApplicationContext();
        for (String languageTag : ProductConfig.LOCALES) {
            var localeContext = createContextForLocale(context, languageTag);
            IncognitoDescriptionView.getSpannedBulletText(
                    localeContext, R.string.new_tab_otr_not_saved);
            IncognitoDescriptionView.getSpannedBulletText(
                    localeContext, R.string.new_tab_otr_visible);
        }
    }

    private String getIncognitoNtpDescriptionSpanned(String description) {
        return SpanApplier.applySpans(description, new SpanInfo("<link>", "</link>", new Object()))
                .toString();
    }
}
