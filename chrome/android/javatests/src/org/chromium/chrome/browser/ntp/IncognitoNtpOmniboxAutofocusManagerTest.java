// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** Tests for {@link IncognitoNtpOmniboxAutofocusManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class IncognitoNtpOmniboxAutofocusManagerTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = null;
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(null);
        setAccessibilityEnabled(false);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenRegularNtpOpened_autofocusFails() {
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, false);

        verifyOmniboxFocusAndKeyboardVisibility(false, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenReturnedAfterNavigating_autofocusFails() {
        // Open an incognito NTP.
        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);

        clearOmniboxFocusOnIncognitoNtp();

        verifyOmniboxFocusAndKeyboardVisibility(false, null);

        // Navigate away. Autofocus should never be triggered again.
        mActivityTestRule.loadUrl("about:blank");

        verifyOmniboxFocusAndKeyboardVisibility(false, null);

        // Return to NTP after navigating.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        verifyOmniboxFocusAndKeyboardVisibility(false, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenLaunchAsNotNtpFirst_autofocusFails() {
        // Open a non-NTP incognito tab.
        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab("about:blank", true);

        verifyOmniboxFocusAndKeyboardVisibility(false, null);

        // Navigate to the NTP.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        // The omnibox should not be focused, as this tab was not opened as an NTP first.
        verifyOmniboxFocusAndKeyboardVisibility(false, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenLaunchFromTabSwitcher_autofocusSucceeds() {
        // Open an incognito tab to select incognito tab model.
        mActivityTestRule.loadUrlInNewTab("about:blank", true);

        // Open the tab switcher.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);

        // Open a new incognito NTP.
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(
                        UrlConstants.NTP_URL, true, TabLaunchType.FROM_TAB_SWITCHER_UI);

        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    public void whenVeryFirstTabOpened_andNotFirstTabEnabled_autofocusFails() {
        for (int i = 0; i < 4; i++) {
            // With the not_first_tab feature enabled, autofocus should be skipped on the first
            // incognito tab, but triggered on any subsequent ones.
            final boolean isFirstTab = i == 0;

            final Tab incognitoNtpTab =
                    mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
            verifyOmniboxFocusAndKeyboardVisibility(!isFirstTab, incognitoNtpTab);

            clearOmniboxFocusOnIncognitoNtp();
            verifyOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);
        }
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    public void whenNotEnoughSpaceWithPrediction_autofocusFails() {
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = false;

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    public void whenEnoughSpaceWithPrediction_autofocusSucceeds() {
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = true;

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void whenHardwareKeyboardAttached_andWithHardwareKeyboardEnabled_autofocusSucceeds() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void whenHardwareKeyboardNotAttached_andWithHardwareKeyboardEnabled_autofocusFails() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true")
    public void whenVeryFirstTabOpenedAndEnoughSpaceWithPrediction_autofocusSucceeds() {
        // There is enough free space on incognito NTP for prediction, it should autofocus.
        IncognitoNtpOmniboxAutofocusManager.sAutofocusAllowedWithPredictionForTesting = true;

        // Open the first incognito tab. With the not_first_tab feature, it should not autofocus.
        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        // Omnibox should be autofocused, because it triggers if any of conditions are met.
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenAccessibilityToggled_autofocusBehaviorChanges() {
        // By default, accessibility is disabled. Autofocus should work.
        final Tab incognitoNtpTab1 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab1);

        // Enable accessibility.
        setAccessibilityEnabled(true);

        // Open another incognito NTP. Autofocus should be disabled.
        final Tab incognitoNtpTab2 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab2);

        // Disable accessibility again.
        setAccessibilityEnabled(false);

        // Open a third incognito NTP. Autofocus should be enabled again.
        final Tab incognitoNtpTab3 = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab3);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenAutofocusManagerInitializedWithExistingTab_autofocusSucceeds() {
        // Autofocus works on a new launched Incognito tab.
        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
        clearOmniboxFocusOnIncognitoNtp();

        // Unregister autofocus observers, by enabling accessibility.
        setAccessibilityEnabled(true);

        // Re-register autofocus observers. This simulates a new autofocus manager being created.
        setAccessibilityEnabled(false);

        // The manager should detect the existing Incognito NTP and trigger autofocus again.
        verifyOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    private void verifyOmniboxFocusAndKeyboardVisibility(boolean enabled, @Nullable Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            enabled
                                    ? "Omnibox should be focused."
                                    : "Omnibox should not be focused.",
                            mActivity.getToolbarManager().isUrlBarFocused(),
                            Matchers.is(enabled));

                    if (tab != null && tab.getView() != null) {
                        Criteria.checkThat(
                                enabled
                                        ? "Keyboard should be visible."
                                        : "Keyboard should not be visible.",
                                KeyboardVisibilityDelegate.getInstance()
                                        .isKeyboardShowing(tab.getView()),
                                Matchers.is(enabled));
                    }
                });
    }

    private void setAccessibilityEnabled(boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoNtpOmniboxAutofocusManager.setAutofocusEnabledForTesting(!enabled);
                });
    }

    private void clearOmniboxFocusOnIncognitoNtp() {
        // Clear focus by tapping on the NTP scroll view.
        Espresso.onView(ViewMatchers.withId(R.id.ntp_scrollview)).perform(ViewActions.click());
    }
}
