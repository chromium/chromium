// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.view.KeyEvent;

import androidx.fragment.app.Fragment;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SupportedProfileType;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxPedal;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxPedalId;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.Arrays;
import java.util.List;

/** Tests of the Omnibox Pedals feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class OmniboxPedalsTest {
    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mActivityTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private WebPageStation mStartingPage;
    private OmniboxTestUtils mOmniboxUtils;
    private SettingsActivity mTargetActivity;

    @Before
    public void setUp() throws InterruptedException {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(false);
        mStartingPage = mActivityTestRule.start();
        mOmniboxUtils = new OmniboxTestUtils(mStartingPage.getActivity());
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoTabHostUtils.closeAllIncognitoTabs();
                });
        if (mTargetActivity != null) {
            ApplicationTestUtils.finishActivity(mTargetActivity);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getModalDialogManager()
                                .dismissAllDialogs(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));
        AutocompleteControllerJni.setInstanceForTesting(null);
    }

    /**
     * Apply suggestions to the Omnibox. Requires at least one of the suggestions to include at
     * least one OmniboxAction. Verifies that suggestions - and actions - are shown.
     *
     * @param matches the matches to show
     */
    private void setSuggestions(AutocompleteMatch... matches) {
        mOmniboxUtils.requestFocus();
        // Ensure we start from empty suggestions list; don't carry over suggestions from previous
        // run.
        mOmniboxUtils.setSuggestions(AutocompleteResult.fromCache(null, null));

        mOmniboxUtils.setSuggestions(AutocompleteResult.fromCache(Arrays.asList(matches), null));
        mOmniboxUtils.checkSuggestionsShown();
        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull("No suggestions with actions", info);
    }

    private AutocompleteMatch createPedalSuggestion(@OmniboxPedalId int pedalId) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Suggestion")
                .setActions(
                        List.of(new OmniboxPedal(pedalId, "hint", "accessibilityHint", pedalId)))
                .build();
    }

    /**
     * Click on the pedal to open the SettingsActivity, and confirm Settings activity is shown
     * presenting the expected fragment.
     *
     * @param fragmentType The class type of the displayed settings fragment.
     */
    private void clickOnPedalToSettings(Runnable activate, Class<? extends Fragment> fragmentType) {
        mTargetActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SettingsActivity.class,
                        activate);

        CriteriaHelper.pollUiThread(
                () -> {
                    Fragment fragment =
                            mTargetActivity
                                    .getSupportFragmentManager()
                                    .findFragmentById(R.id.content);
                    Criteria.checkThat(fragment, Matchers.instanceOf(fragmentType));
                });
    }

    @Test
    @MediumTest
    public void testClearBrowsingData_withQuickDeleteEnabled() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.CLEAR_BROWSING_DATA));
        mOmniboxUtils.clickOnAction(0, 0);

        onViewWaiting(withId(R.id.quick_delete_spinner)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testManagePaymentMethods() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.UPDATE_CREDIT_CARD));
        clickOnPedalToSettings(
                () -> mOmniboxUtils.clickOnAction(0, 0), AutofillPaymentMethodsFragment.class);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenIncognitoTab_Phone() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.LAUNCH_INCOGNITO));

        mOmniboxUtils.clickOnAction(0, 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(tab.isIncognito(), Matchers.is(true));
                });
    }

    @Test
    @MediumTest
    @ImportantFormFactors(DeviceFormFactor.ONLY_TABLET)
    @Restriction({DeviceFormFactor.ONLY_TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenIncognitoTab_Tablet() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.LAUNCH_INCOGNITO));

        mOmniboxUtils.clickOnAction(0, 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    List<Activity> activities = ApplicationStatus.getRunningActivities();
                    ChromeTabbedActivity cta = null;
                    int regularCta = 0;
                    int incognitoCta = 0;
                    for (Activity activity : activities) {
                        if (activity instanceof ChromeTabbedActivity) {
                            cta = (ChromeTabbedActivity) activity;
                            if (cta.getSupportedProfileType() == SupportedProfileType.REGULAR) {
                                regularCta++;
                            }
                            if (cta.getSupportedProfileType()
                                    == SupportedProfileType.OFF_THE_RECORD) {
                                incognitoCta++;
                            }
                        }
                    }
                    return regularCta == 1 && incognitoCta == 1;
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testOpenChromeSafetyHub() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.RUN_CHROME_SAFETY_CHECK));

        clickOnPedalToSettings(() -> mOmniboxUtils.clickOnAction(0, 0), SafetyHubFragment.class);
    }

    @Test
    @MediumTest
    public void testManageSiteSettings() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.MANAGE_SITE_SETTINGS));
        clickOnPedalToSettings(() -> mOmniboxUtils.clickOnAction(0, 0), SiteSettings.class);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testManageChromeSettings() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.MANAGE_CHROME_SETTINGS));

        clickOnPedalToSettings(() -> mOmniboxUtils.clickOnAction(0, 0), MainSettings.class);
    }

    @Test
    @MediumTest
    public void testViewYourChromeHistory() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.VIEW_CHROME_HISTORY));

        mOmniboxUtils.clickOnAction(0, 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(
                            tab.getUrl().getSpec(),
                            Matchers.startsWith(UrlConstants.NATIVE_HISTORY_URL));
                });
    }

    @Test
    @MediumTest
    public void testManageAccessibilitySettings() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY));
        clickOnPedalToSettings(
                () -> mOmniboxUtils.clickOnAction(0, 0), AccessibilitySettings.class);
    }

    @Test
    @MediumTest
    public void testPedalsStartedOnTabEnterKeyStroke() throws Exception {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY));

        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_TAB));
        clickOnPedalToSettings(
                () -> {
                    onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
                },
                AccessibilitySettings.class);
    }

    @Test
    @MediumTest
    public void testPlayChromeDinoGame() throws InterruptedException {
        setSuggestions(createPedalSuggestion(OmniboxPedalId.PLAY_CHROME_DINO_GAME));

        mOmniboxUtils.clickOnAction(0, 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(
                            tab.getUrl().getSpec(), Matchers.equalTo(UrlConstants.CHROME_DINO_URL));
                });
    }
}
