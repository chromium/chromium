// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment.CARD_BENEFITS_LEARN_MORE_CLICKED_USER_ACTION;
import static org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment.CARD_BENEFITS_TOGGLED_OFF_USER_ACTION;
import static org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment.CARD_BENEFITS_TOGGLED_ON_USER_ACTION;
import static org.chromium.chrome.browser.autofill.settings.AutofillCardBenefitsFragment.PREF_KEY_ENABLE_CARD_BENEFIT;
import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for AutofillCardBenefitsFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE
})
public class AutofillCardBenefitsFragmentTest {
    @Rule public final AutofillTestRule mRule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillCardBenefitsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillCardBenefitsFragment.class);

    private AutofillTestHelper mAutofillTestHelper;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
        mActionTester.tearDown();
    }

    // Test to verify that the Preference screen is displayed and its title is visible as expected.
    @Test
    @MediumTest
    public void testCardBenefitsPreferenceScreen_shownWithTitle() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNotNull(getPreferenceScreen(activity));
        Assert.assertEquals(
                activity.getTitle().toString(),
                activity.getString(R.string.autofill_card_benefits_settings_page_title));
    }

    // Test to verify that the Preference screen is displayed and the enable benefits toggle is
    // visible as expected when benefit is enabled.
    @Test
    @MediumTest
    public void testCardBenefitsPreferenceScreen_ToggleShownAndEnabled() throws Exception {
        // Initial state, card benefits is enabled by default.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map are only enable/disable benefits
        // toggle with enabled status and learn about link.
        Assert.assertEquals(2, getPreferenceScreen(activity).getPreferenceCount());
        ChromeSwitchPreference benefitTogglePreference =
                (ChromeSwitchPreference)
                        getPreferenceScreen(activity).findPreference(PREF_KEY_ENABLE_CARD_BENEFIT);
        Assert.assertEquals(
                benefitTogglePreference.getTitle(),
                activity.getString(R.string.autofill_settings_page_card_benefits_label));
        Assert.assertEquals(
                benefitTogglePreference.getSummary(),
                activity.getString(R.string.autofill_settings_page_card_benefits_toggle_summary));
        Assert.assertTrue(benefitTogglePreference.isEnabled());
        Assert.assertTrue(benefitTogglePreference.isChecked());
    }

    // Test to verify that the Preference screen is displayed and the enable benefits toggle is
    // visible as expected when benefit is disabled.
    @Test
    @MediumTest
    public void testCardBenefitsPreferenceScreen_ToggleShownAndDisabled() throws Exception {
        // Initial state, card benefits is enabled by default.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS, false);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map are only enable/disable benefits
        // toggle with disabled status and learn about link.
        Assert.assertEquals(2, getPreferenceScreen(activity).getPreferenceCount());
        ChromeSwitchPreference benefitTogglePreference =
                (ChromeSwitchPreference)
                        getPreferenceScreen(activity).findPreference(PREF_KEY_ENABLE_CARD_BENEFIT);
        Assert.assertTrue(benefitTogglePreference.isEnabled());
        Assert.assertFalse(benefitTogglePreference.isChecked());
    }

    // Test to verify that the toggle status is linked with the correct preference and user
    // interaction.
    @Test
    @MediumTest
    public void testCardBenefitsPreferenceScreen_toggleClicked() throws Exception {
        // Initial state, card benefits is enabled by default.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference benefitTogglePreference =
                (ChromeSwitchPreference)
                        getPreferenceScreen(activity).findPreference(PREF_KEY_ENABLE_CARD_BENEFIT);

        // Simulate clicks to turn off and back on benefits.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    benefitTogglePreference.performClick();
                    Assert.assertFalse(
                            getPrefService().getBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS));
                    Assert.assertFalse(benefitTogglePreference.isChecked());
                    Assert.assertTrue(
                            "User action should be logged when the user toggles card benefits off.",
                            mActionTester
                                    .getActions()
                                    .contains(CARD_BENEFITS_TOGGLED_OFF_USER_ACTION));

                    benefitTogglePreference.performClick();
                    Assert.assertTrue(
                            getPrefService().getBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS));
                    Assert.assertTrue(benefitTogglePreference.isChecked());
                    Assert.assertTrue(
                            "User action should be logged when the user toggles card benefits on.",
                            mActionTester
                                    .getActions()
                                    .contains(CARD_BENEFITS_TOGGLED_ON_USER_ACTION));
                });
    }

    // Test to verify that learn more text contains the expected text content.
    @Test
    @MediumTest
    public void testCardBenefitsPreferenceScreen_learnMoreLink() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to card benefits toggle + learn
        // about link preference.
        Assert.assertEquals(2, getPreferenceScreen(activity).getPreferenceCount());

        Preference linkPreference = getPreferenceScreen(activity).getPreference(1);
        Assert.assertEquals(
                linkPreference.getSummary().toString(),
                activity.getString(
                                R.string.autofill_settings_page_card_benefits_learn_about_link_text)
                        .replaceAll("<.?link>", ""));
        onView(withText(containsString("Learn more"))).perform(clickOnClickableSpan(0));
        Assert.assertTrue(
                "User action should be logged when the user clicks on learn more link.",
                mActionTester.getActions().contains(CARD_BENEFITS_LEARN_MORE_CLICKED_USER_ACTION));
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillCardBenefitsFragment) activity.getMainFragment()).getPreferenceScreen();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }
}
