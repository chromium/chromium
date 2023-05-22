// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.app.Activity;
import android.view.KeyEvent;

import androidx.fragment.app.Fragment;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxPedal;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.components.omnibox.action.OmniboxPedalId;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests of the Omnibox Pedals feature.
 */
@RunWith(ParameterizedRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class OmniboxPedalsTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            List.of(new ParameterSet().value(false).name("RegularTab"),
                    new ParameterSet().value(true).name("IncognitoTab"));

    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public static @ClassRule DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    private OmniboxTestUtils mOmniboxUtils;
    private boolean mIncognito;
    private LocationBarLayout mLocationBarLayout;
    private Activity mTargetActivity;

    public OmniboxPedalsTest(boolean incognito) {
        mIncognito = incognito;
    }

    @BeforeClass
    public static void beforeClass() {
        FeatureList.TestValues featureTestValues = new FeatureList.TestValues();
        featureTestValues.addFeatureFlagOverride(ChromeFeatureList.HISTORY_JOURNEYS, true);
        FeatureList.setTestValues(featureTestValues);
        FeatureList.setTestCanUseDefaultsForTesting();

        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUp() throws InterruptedException {
        if (mIncognito) {
            sActivityTestRule.newIncognitoTabFromMenu();
        }
        sActivityTestRule.loadUrl("about:blank");
        mOmniboxUtils = new OmniboxTestUtils(sActivityTestRule.getActivity());
        mLocationBarLayout =
                (LocationBarLayout) sActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    @After
    public void tearDown() throws Exception {
        if (mOmniboxUtils.getFocus()) {
            mOmniboxUtils.clearFocus();
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { IncognitoTabHostUtils.closeAllIncognitoTabs(); });
        if (mTargetActivity != null) {
            ApplicationTestUtils.finishActivity(mTargetActivity);
        }
    }

    /**
     * Type the |text| into |activity|'s url_bar.
     *
     * @param activity The Activity which url_bar is in.
     * @param text The text will be typed into url_bar.
     */
    private void typeInOmnibox(String text) throws InterruptedException {
        mOmniboxUtils.requestFocus();
        mOmniboxUtils.setText("");
        mOmniboxUtils.typeText(text, false);
        mOmniboxUtils.waitForAutocomplete();
        mOmniboxUtils.checkSuggestionsShown();
    }

    /**
     * Find the Omnibox Pedal suggestion which suggests the |pedalId|, and return the
     * suggestion. This method needs to run on the UI thread.
     *
     * @return The suggestion which suggests the matching OmniboxPedalId.
     */
    private AutocompleteMatch findOmniboxPedalSuggestion() {
        ThreadUtils.assertOnUiThread();
        AutocompleteCoordinator coordinator = mLocationBarLayout.getAutocompleteCoordinator();
        // Find the first matching suggestion.
        for (int i = 0; i < coordinator.getSuggestionCount(); ++i) {
            AutocompleteMatch suggestion = coordinator.getSuggestionAt(i);
            if (suggestion != null && !suggestion.getActions().isEmpty()
                    && suggestion.getActions().get(0).actionId == OmniboxActionId.PEDAL) {
                return suggestion;
            }
        }
        return null;
    }

    private void clickOnPedal() {
        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        CriteriaHelper.pollUiThread(() -> {
            var adapter = info.view.getActionChipsView().getAdapter();
            adapter.selectNextItem();
            adapter.getSelectedView().performClick();
        }, DEFAULT_MAX_TIME_TO_POLL * 5, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Check whether the |pedalId| pedal suggestion was shown.
     *
     * @param pedalId The Omnibox pedal type to be found.
     * @param expectShown expect pedal is shown or not.
     */
    private void checkPedalWasShown(@OmniboxPedalId int pedalId, boolean expectShown) {
        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion();
            Criteria.checkThat(
                    matchSuggestion, expectShown ? Matchers.notNullValue() : Matchers.nullValue());
        });
    }

    /**
     * click on the pedal to open the SettingsActivity, and return the SettingsActivity.
     *
     * @param activityType The class type of the activity.
     * @return The opened SettingsActivity.
     */
    private <T> T clickOnPedalToSettings(final Class<T> activityType) {
        mTargetActivity = (Activity) ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), activityType, () -> clickOnPedal());

        return (T) mTargetActivity;
    }

    /**
     * Create a HistogramWatcher expecting both the shown and used histograms are recorded.
     *
     * @param pedalId The Omnibox pedal type.
     */
    private HistogramWatcher newHistogramExpectations(@OmniboxPedalId int pedalId) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord("Omnibox.PedalShown", pedalId)
                .expectIntRecord("Omnibox.SuggestionUsed.Pedal", pedalId)
                .build();
    }

    /**
     * Check the settings fragment is shownm, and the Omnibox did not hold the focus.
     *
     * @param settingsActivity A Settings activity containing the fragment been shown.
     * @param fragmentClass The fragment should be shown.
     */
    private void checkSettingsWasShownAndOmniboxNoFocus(
            SettingsActivity settingsActivity, Class<? extends Fragment> fragmentClass) {
        CriteriaHelper.pollUiThread(() -> {
            Fragment fragment =
                    settingsActivity.getSupportFragmentManager().findFragmentById(R.id.content);
            Criteria.checkThat(fragment, Matchers.instanceOf(fragmentClass));
        });
        mOmniboxUtils.checkFocus(false);
    }

    /**
     * Build a dummy suggestions list.
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     *
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildDummySuggestionsList(int count, String prefix) {
        List<AutocompleteMatch> list = new ArrayList<>();
        for (int index = 0; index < count; ++index) {
            list.add(AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                             .setDisplayText(prefix + (index + 1))
                             .build());
        }

        return list;
    }

    /**
     * Create a dummy pedal suggestion.
     * @param name The dummy suggestion name.
     * @param id The Omnibox pedal type to be created.
     *
     * @return a dummy pedal suggestion.
     */
    private AutocompleteMatch createDummyPedalSuggestion(String name, @OmniboxPedalId int id) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText(name)
                .setActions(List.of(new OmniboxPedal("hint", id)))
                .build();
    }

    @Test
    @MediumTest
    public void testClearBrowsingDataOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.CLEAR_BROWSING_DATA);

        // Generate the clear browsing data pedal.
        typeInOmnibox("Clear data");

        checkPedalWasShown(OmniboxPedalId.CLEAR_BROWSING_DATA, /*expectShown=*/!mIncognito);

        if (mIncognito) {
            // In incognito mode, no pedal shows, so the test can stop here.
            return;
        }

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(
                settingsActivity, ClearBrowsingDataTabsFragment.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testManagePasswordsOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.MANAGE_PASSWORDS);

        // Generate the manage passwords pedal.
        typeInOmnibox("Manage passwords");

        checkPedalWasShown(OmniboxPedalId.MANAGE_PASSWORDS, /*expectShown=*/true);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(settingsActivity, PasswordSettings.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testManagePaymentMethodsOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.UPDATE_CREDIT_CARD);

        // Generate the manage payment methods pedal.
        typeInOmnibox("Manage payment methods");

        checkPedalWasShown(OmniboxPedalId.UPDATE_CREDIT_CARD, /*expectShown=*/true);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(
                settingsActivity, AutofillPaymentMethodsFragment.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1434836 - IncognitoTab version is flaky")
    public void testOpenIncognitoTabOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.LAUNCH_INCOGNITO);

        // Generate the open incognito pedal.
        typeInOmnibox("Open Incognito");

        checkPedalWasShown(OmniboxPedalId.LAUNCH_INCOGNITO, /*expectShown=*/true);

        clickOnPedal();

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(tab.isIncognito(), Matchers.is(true));
        });

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRunChromeSafetyCheckOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.RUN_CHROME_SAFETY_CHECK);
        HistogramWatcher safetyCheckHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Settings.SafetyCheck.UpdatesResult")
                        .build();

        // Generate the run chrome safety check pedal.
        typeInOmnibox("Run safety check");

        checkPedalWasShown(OmniboxPedalId.RUN_CHROME_SAFETY_CHECK, /*expectShown=*/true);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(settingsActivity, SafetyCheckSettingsFragment.class);

        histogramWatcher.assertExpected();

        // Make sure the safety check was ran.
        safetyCheckHistogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    public void testManageSiteSettingsOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.MANAGE_SITE_SETTINGS);

        // Generate the manage site setting pedal.
        typeInOmnibox("Change site permissions");

        checkPedalWasShown(OmniboxPedalId.MANAGE_SITE_SETTINGS, /*expectShown=*/true);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(settingsActivity, SiteSettings.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testManageChromeSettingsOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.MANAGE_CHROME_SETTINGS);

        // Generate the manage chrome settings pedal.
        typeInOmnibox("manage settings");

        checkPedalWasShown(OmniboxPedalId.MANAGE_CHROME_SETTINGS, /*expectShown=*/true);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(settingsActivity, MainSettings.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testViewYourChromeHistoryOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.VIEW_CHROME_HISTORY);

        // Generate the view chrome history pedal.
        typeInOmnibox("view chrome history");

        checkPedalWasShown(OmniboxPedalId.VIEW_CHROME_HISTORY, /*expectShown=*/true);

        clickOnPedal();
        CriteriaHelper.pollUiThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(
                    tab.getUrl().getSpec(), Matchers.startsWith(UrlConstants.HISTORY_URL));
        });

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testManageAccessibilitySettingsOmniboxPedalSuggestion()
            throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY);

        // Generate the manage accessibility setting pedal.
        typeInOmnibox("Chrome accessibility");

        checkPedalWasShown(OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY,
                /*expectShown=*/true);

        SettingsActivity settingsActivity = clickOnPedalToSettings(SettingsActivity.class);
        Assert.assertNotNull("Could not find the Settings activity", settingsActivity);

        checkSettingsWasShownAndOmniboxNoFocus(settingsActivity, AccessibilitySettings.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPedalsStartedOnCtrlEnterKeyStroke() throws Exception {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY);

        typeInOmnibox("Chrome accessibility");
        SuggestionInfo<BaseSuggestionView> pedal = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull(pedal.view);
        mOmniboxUtils.focusSuggestion(pedal.index);

        // Select Pedal with the TAB key and activate it with an ENTER key.
        mOmniboxUtils.sendKey(KeyEvent.KEYCODE_TAB);

        mTargetActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SettingsActivity.class,
                () -> mOmniboxUtils.sendKey(KeyEvent.KEYCODE_ENTER));
        Assert.assertNotNull("Could not find the Settings activity", mTargetActivity);

        checkSettingsWasShownAndOmniboxNoFocus(
                (SettingsActivity) mTargetActivity, AccessibilitySettings.class);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPlayChromeDinoGameOmniboxPedalSuggestion() throws InterruptedException {
        HistogramWatcher histogramWatcher =
                newHistogramExpectations(OmniboxPedalId.PLAY_CHROME_DINO_GAME);

        // Generate the play chrome dino game pedal.
        typeInOmnibox("Dino game");

        checkPedalWasShown(OmniboxPedalId.PLAY_CHROME_DINO_GAME, /*expectShown=*/true);

        // Click the pedal.
        clickOnPedal();

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            Criteria.checkThat(
                    tab.getUrl().getSpec(), Matchers.equalTo(UrlConstants.CHROME_DINO_URL));
        });

        histogramWatcher.assertExpected();
    }

    @Test(expected = AssertionError.class)
    @MediumTest
    public void testNoPedalSuggestionAfterTop3() {
        mOmniboxUtils.requestFocus();
        List<AutocompleteMatch> suggestionsList = buildDummySuggestionsList(3, "Suggestion");
        suggestionsList.add(
                createDummyPedalSuggestion("pedal", OmniboxPedalId.CLEAR_BROWSING_DATA));

        mOmniboxUtils.setSuggestions(
                AutocompleteResult.fromCache(suggestionsList, null), "Suggestion");
        mOmniboxUtils.checkSuggestionsShown();

        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNull(
                "Should not show pedals if the suggestion is not in top 3 suggestions", info);
    }

    @Test
    @MediumTest
    public void testShownPedalSuggestionInTop3() {
        mOmniboxUtils.requestFocus();
        List<AutocompleteMatch> suggestionsList = buildDummySuggestionsList(2, "Suggestion");
        suggestionsList.add(
                createDummyPedalSuggestion("pedal", OmniboxPedalId.CLEAR_BROWSING_DATA));

        mOmniboxUtils.setSuggestions(
                AutocompleteResult.fromCache(suggestionsList, null), "Suggestion");
        mOmniboxUtils.checkSuggestionsShown();

        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull("Should show a pedal if the suggestion is in top 3 suggestions", info);
    }
}
