// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import androidx.fragment.app.Fragment;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionView;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.chrome.test.util.WaitForFocusHelper;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.test.util.UiDisableIf;

/**
 * Tests of the Omnibox Pedals feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OmniboxPedalsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private OmniboxTestUtils mOmniboxUtils;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mOmniboxUtils = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    /**
     * Type the |text| into |activity|'s url_bar.
     *
     * @param activity The Activity which url_bar is in.
     * @param text The text will be typed into url_bar.
     */
    private void typeInOmnibox(Activity activity, String text) throws InterruptedException {
        final UrlBar urlBar = activity.findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        WaitForFocusHelper.acquireFocusForView(urlBar);
        mOmniboxUtils.requestFocus();

        mOmniboxUtils.typeText(text, false);
        mOmniboxUtils.checkSuggestionsShown();
    }

    /**
     * Find the Omnibox Pedal suggestion which suggests the |pedalType|, and return the
     * suggestion. This method needs to run on the UI thread.
     *
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param pedalType The Omnibox pedal type to be found.
     * @return The suggesstion which suggests the matching OmniboxPedalType.
     */
    private AutocompleteMatch findOmniboxPedalSuggestion(
            LocationBarLayout locationBarLayout, @OmniboxPedalType int pedalType) {
        ThreadUtils.assertOnUiThread();
        AutocompleteCoordinator coordinator = locationBarLayout.getAutocompleteCoordinator();
        // Find the first matching suggestion.
        for (int i = 0; i < coordinator.getSuggestionCount(); ++i) {
            AutocompleteMatch suggestion = coordinator.getSuggestionAt(i);
            if (suggestion != null && suggestion.getOmniboxPedal() != null
                    && suggestion.getOmniboxPedal().getID() == pedalType) {
                return suggestion;
            }
        }
        return null;
    }

    private void clickOnPedal(
            LocationBarLayout locationBarLayout, @OmniboxPedalType int omniboxPedalType) {
        SuggestionInfo<PedalSuggestionView> info =
                mOmniboxUtils.getSuggestionByType(OmniboxSuggestionUiType.PEDAL_SUGGESTION);
        CriteriaHelper.pollUiThread(() -> {
            TestTouchUtils.performClickOnMainSync(
                    InstrumentationRegistry.getInstrumentation(), info.view.getPedalChipView());
        }, DEFAULT_MAX_TIME_TO_POLL * 5, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Ensure the |pedalType| pedal suggestion was shown.
     *
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param pedalType The Omnibox pedal type to be found.
     */
    private void ensurePedalWasShown(
            LocationBarLayout locationBarLayout, @OmniboxPedalType int pedalType) {
        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion =
                    findOmniboxPedalSuggestion(locationBarLayout, pedalType);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    /**
     * click on the pedal to open the SettingsActivity, and return the SettingsActivity.
     *
     * @param activityType The class type of the activity.
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param pedalType The Omnibox pedal type to be found.
     * @return The opened SettingsActivity.
     */
    private <T> T clickOnPedalToSettings(final Class<T> activityType,
            LocationBarLayout locationBarLayout, @OmniboxPedalType int pedalType) {
        return ActivityTestUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                activityType, () -> { clickOnPedal(locationBarLayout, pedalType); });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testClearBrowsingDataOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the clear browsing data pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Clear data");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.CLEAR_BROWSING_DATA);

        SettingsActivity settingsActivity = clickOnPedalToSettings(
                SettingsActivity.class, locationBarLayout, OmniboxPedalType.CLEAR_BROWSING_DATA);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the clear browsing data setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(ClearBrowsingDataTabsFragment.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManagePasswordsOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the manage passwords pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Manage passwords");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.MANAGE_PASSWORDS);

        SettingsActivity settingsActivity = clickOnPedalToSettings(
                SettingsActivity.class, locationBarLayout, OmniboxPedalType.MANAGE_PASSWORDS);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the manage passwords setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(PasswordSettings.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManagePaymentMethodsOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the manage payment methods pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Manage payment methods");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.UPDATE_CREDIT_CARD);

        SettingsActivity settingsActivity = clickOnPedalToSettings(
                SettingsActivity.class, locationBarLayout, OmniboxPedalType.UPDATE_CREDIT_CARD);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the manage passwords setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(AutofillPaymentMethodsFragment.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testOpenIncognitoTabOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the open incognito pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Open Incognito");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.LAUNCH_INCOGNITO);

        clickOnPedal(locationBarLayout, OmniboxPedalType.LAUNCH_INCOGNITO);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab.isIncognito(), Matchers.is(true));
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testRunChromeSafetyCheckOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the run chrome safety check pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Run safety check");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.RUN_CHROME_SAFETY_CHECK);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class,
                locationBarLayout, OmniboxPedalType.RUN_CHROME_SAFETY_CHECK);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the chrome safety check setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(SafetyCheckSettingsFragment.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManageSiteSettingsOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the manage site setting pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Change site permissions");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.MANAGE_SITE_SETTINGS);

        SettingsActivity settingsActivity = clickOnPedalToSettings(
                SettingsActivity.class, locationBarLayout, OmniboxPedalType.MANAGE_SITE_SETTINGS);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the manage site setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(SiteSettings.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManageChromeSettingsOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the manage chrome settings pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "manage settings");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.MANAGE_CHROME_SETTINGS);

        SettingsActivity settingsActivity = clickOnPedalToSettings(
                SettingsActivity.class, locationBarLayout, OmniboxPedalType.MANAGE_CHROME_SETTINGS);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the chrome setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(MainSettings.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1291207
    public void testViewYourChromeHistoryOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the view chrome history pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "view chrome history");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.VIEW_CHROME_HISTORY);

        HistoryActivity historyActivity = clickOnPedalToSettings(
                HistoryActivity.class, locationBarLayout, OmniboxPedalType.VIEW_CHROME_HISTORY);

        // Make sure the history setting page was opened.
        Assert.assertNotNull("Could not find the history activity", historyActivity);

        historyActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManageAccessibilitySettingsOmniboxPedalSuggestion()
            throws InterruptedException {
        // Generate the manage accessibility setting pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Chrome accessibility");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class,
                locationBarLayout, OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        // Make sure the manage accessibility setting page was opened.
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(AccessibilitySettings.class));
        });

        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testPlayChromeDinoGameOmniboxPedalSuggestion() throws InterruptedException {
        // Generate the play chrome dino game pedal.
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Dino game");

        ensurePedalWasShown(locationBarLayout, OmniboxPedalType.PLAY_CHROME_DINO_GAME);

        // Click the pedal.
        clickOnPedal(locationBarLayout, OmniboxPedalType.PLAY_CHROME_DINO_GAME);

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(
                    tab.getUrl().getSpec(), Matchers.startsWith(UrlConstants.CHROME_DINO_URL));
        });
    }
}
