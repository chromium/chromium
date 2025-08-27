// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.After;
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
    ChromeFeatureList.IP_PROTECTION_UX,
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
    @DisableFeatures({
        ChromeFeatureList.FINGERPRINTING_PROTECTION_UX,
        ChromeFeatureList.IP_PROTECTION_UX
    })
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
