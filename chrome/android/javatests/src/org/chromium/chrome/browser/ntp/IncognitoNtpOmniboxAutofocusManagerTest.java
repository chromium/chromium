// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.view.View;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.TabSwitcherActionMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for {@link IncognitoNtpOmniboxAutofocusManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/479863847
public class IncognitoNtpOmniboxAutofocusManagerTest {
    /**
     * The maximum time to wait for omnibox focus and keyboard visibility. On some devices the
     * software keyboard is slow to appear.
     */
    private static final long VERIFY_FOCUS_MAX_TIME_TO_POLL_MS = 30000L;

    /** The polling interval to wait between checking for omnibox focus and keyboard visibility. */
    private static final long VERIFY_FOCUS_POLLING_INTERVAL_MS = 50;

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_INCOGNITO)
                    .setRevision(3)
                    .setDescription("Updated Incognito splash to GM3")
                    .build();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mInitialPage;

    @Before
    public void setUp() {
        mInitialPage = mActivityTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        setAccessibilityEnabled(false);
        mActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void whenRegularNtpOpened_autofocusFails() {
        mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), false);

        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void whenReturnedAfterNavigating_autofocusFails_phone() {
        // Open an incognito NTP.
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);

        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);

        clearOmniboxFocusOnIncognitoNtp();

        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);

        // Navigate away. Autofocus should never be triggered again.
        mActivityTestRule.loadUrl("about:blank");

        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);

        // Return to NTP after navigating.
        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void whenReturnedAfterNavigating_autofocusFails_tabletOrDesktopNonAuto() {
        // Open an incognito NTP.
        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);

        clearOmniboxFocusOnIncognitoNtp();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, ntpPage);

        // Navigate away. Autofocus should never be triggered again.
        WebPageStation webPage = ntpPage.loadWebPageProgrammatically("about:blank");
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, webPage);

        // Return to NTP after navigating.
        ntpPage =
                webPage.loadPageProgrammatically(
                        getOriginalNativeNtpUrl(), IncognitoNewTabPageStation.newBuilder());
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void whenLaunchAsNotNtpFirst_autofocusFails_phone() {
        // Open a non-NTP incognito tab.
        final Tab incognitoNtpTab = mActivityTestRule.loadUrlInNewTab("about:blank", true);

        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);

        // Navigate to the NTP.
        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        NewTabPageTestUtils.waitForNtpLoaded(incognitoNtpTab);

        // The omnibox should not be focused, as this tab was not opened as an NTP first.
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void whenLaunchAsNotNtpFirst_autofocusFails_tabletOrDesktopNonAuto() {
        // Open a non-NTP incognito tab.
        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        WebPageStation nonNtpPage = ntpPage.openFakeLinkToWebPage("about:blank");

        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, nonNtpPage);

        // Navigate to the NTP.
        ntpPage =
                nonNtpPage.loadPageProgrammatically(
                        getOriginalNativeNtpUrl(), IncognitoNewTabPageStation.newBuilder());

        // The omnibox should not be focused, as this tab was not opened as an NTP first.
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void whenLaunchFromTabSwitcher_autofocusSucceeds_phone() {
        // Open an incognito tab to select incognito tab model.
        mActivityTestRule.loadUrlInNewTab("about:blank", true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);

        // Open the Tab Switcher.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);

        // Open a new incognito NTP.
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(
                        getOriginalNativeNtpUrl(), true, TabLaunchType.FROM_TAB_SWITCHER_UI);

        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void whenLaunchFromTabSwitcher_andNotFirstTabEnabled_autofocusFails_phone() {
        // Delay in waiting for Layout causes second time trying to autofocus, so this test checks
        // whether logic of counting and checking "not_first_tab" condition was not corrupted.

        // Open an incognito tab to select incognito tab model.
        mActivityTestRule.loadUrlInNewTab("about:blank", true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);

        // Open the Tab Switcher.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);

        // Open first incognito NTP from Tab Switcher.
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(
                        getOriginalNativeNtpUrl(), true, TabLaunchType.FROM_TAB_SWITCHER_UI);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);

        // Open second incognito NTP.
        final Tab incognitoNtpTab2 =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab2);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void whenLaunchFromTabSwitcher_autofocusSucceeds_tabletOrDesktopNonAuto() {
        // Open an incognito tab.
        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();

        // Open the tab switcher.
        TabSwitcherActionMenuFacility tabSwitcherFacility = ntpPage.openTabSwitcherActionMenu();

        // Open a new incognito NTP.
        ntpPage =
                tabSwitcherFacility
                        .newIncognitoTabMenuItemElement
                        .clickTo()
                        .arriveAt(
                                IncognitoNewTabPageStation.newBuilder()
                                        .initOpeningNewTab()
                                        .build());
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void whenVeryFirstTabOpened_andNotFirstTabEnabled_autofocusFails_phone() {
        // With the "not_first_tab" feature, the first incognito NTP skips autofocus while
        // subsequent ones do not. Open a non-NTP incognito tab first to ensure it is not
        // counted, so that the first actual NTP correctly skips autofocus.
        mActivityTestRule.loadUrlInNewTab("about:blank", true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, null);

        for (int i = 0; i < 4; i++) {
            final boolean isFirstNtpTab = i == 0;

            final Tab incognitoNtpTab =
                    mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
            verifyPhoneOmniboxFocusAndKeyboardVisibility(!isFirstNtpTab, incognitoNtpTab);

            clearOmniboxFocusOnIncognitoNtp();
            verifyPhoneOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);
        }
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void
            whenVeryFirstTabOpened_andNotFirstTabEnabled_autofocusFails_tabletOrDesktopNonAuto() {
        // With the not_first_tab feature enabled, autofocus should be skipped on the first
        // incognito tab, but triggered on any subsequent ones.
        IncognitoNewTabPageStation initialNtpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, initialNtpPage);

        IncognitoNewTabPageStation currPage = initialNtpPage;
        for (int i = 1; i < 4; i++) {
            IncognitoNewTabPageStation newNtpPage = currPage.openNewIncognitoTabFast();
            verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, newNtpPage);

            clearOmniboxFocusOnIncognitoNtp();
            verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, newNtpPage);
            currPage = newNtpPage;
        }
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void whenNotEnoughSpaceWithPrediction_autofocusFails_phone() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(false);

        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void whenNotEnoughSpaceWithPrediction_autofocusFails_tabletOrDesktop() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(false);

        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void whenEnoughSpaceWithPrediction_autofocusSucceeds_phone() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);

        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void whenEnoughSpaceWithPrediction_autofocusSucceeds_tabletOrDesktopNonAuto() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);

        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void
            whenHardwareKeyboardAttached_andWithHardwareKeyboardEnabled_autofocusSucceeds_phone() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void
            whenHardwareKeyboardAttached_andWithHardwareKeyboardEnabled_autofocusSucceeds_tabletOrDesktopNonAuto() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void
            whenHardwareKeyboardNotAttached_andWithHardwareKeyboardEnabled_autofocusFails_phone() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void
            whenHardwareKeyboardNotAttached_andWithHardwareKeyboardEnabled_autofocusFails_tabletOrDesktopNonAuto() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void whenVeryFirstTabOpenedAndEnoughSpaceWithPrediction_autofocusSucceeds_phone() {
        // There is enough free space on incognito NTP for prediction, it should autofocus.
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);

        // Open the first incognito tab. With the not_first_tab feature, it should not autofocus.
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);

        // Omnibox should be autofocused, because it triggers if any of conditions are met.
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void
            whenVeryFirstTabOpenedAndEnoughSpaceWithPrediction_autofocusSucceeds_tabletOrDesktopNonAuto() {
        // There is enough free space on incognito NTP for prediction, it should autofocus.
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);

        // Open the first incognito tab. With the not_first_tab feature, it should not autofocus.
        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();

        // Omnibox should be autofocused, because it triggers if any of conditions are met.
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void whenAccessibilityToggled_autofocusBehaviorChanges_phone() {
        // By default, accessibility is disabled. Autofocus should work.
        final Tab incognitoNtpTab1 =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab1);

        // Enable accessibility.
        setAccessibilityEnabled(true);

        // Open another incognito NTP. Autofocus should be disabled.
        final Tab incognitoNtpTab2 =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(false, incognitoNtpTab2);

        // Disable accessibility again.
        setAccessibilityEnabled(false);

        // Open a third incognito NTP. Autofocus should be enabled again.
        final Tab incognitoNtpTab3 =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab3);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void whenAccessibilityToggled_autofocusBehaviorChanges_tabletOrDesktopNonAuto() {
        // By default, accessibility is disabled. Autofocus should work.
        IncognitoNewTabPageStation ntpPage1 = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage1);

        // Enable accessibility.
        setAccessibilityEnabled(true);

        // Open another incognito NTP. Autofocus should be disabled.
        IncognitoNewTabPageStation ntpPage2 = ntpPage1.openNewIncognitoTabFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(false, ntpPage2);

        // Disable accessibility again.
        setAccessibilityEnabled(false);

        // Open a third incognito NTP. Autofocus should be enabled again.
        IncognitoNewTabPageStation ntpPage3 = ntpPage2.openNewIncognitoTabFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage3);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void whenAutofocusManagerInitializedWithExistingTab_autofocusSucceeds_phone() {
        // Autofocus works on a new launched Incognito tab.
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
        clearOmniboxFocusOnIncognitoNtp();

        // Unregister autofocus observers, by enabling accessibility.
        setAccessibilityEnabled(true);

        // Re-register autofocus observers. This simulates a new autofocus manager being created.
        setAccessibilityEnabled(false);

        // The manager should detect the existing Incognito NTP and trigger autofocus again.
        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "crbug.com/461578876: Disabled due to flakiness")
    public void
            whenAutofocusManagerInitializedWithExistingTab_autofocusSucceeds_tabletOrDesktopNonAuto() {
        // Autofocus works on a new launched Incognito tab.
        IncognitoNewTabPageStation ntpPage = mInitialPage.openNewIncognitoTabOrWindowFast();
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);
        clearOmniboxFocusOnIncognitoNtp();

        // Unregister autofocus observers, by enabling accessibility.
        setAccessibilityEnabled(true);

        // Re-register autofocus observers. This simulates a new autofocus manager being created.
        setAccessibilityEnabled(false);

        // The manager should detect the existing Incognito NTP and trigger autofocus again.
        verifyNonPhoneOmniboxFocusAndKeyboardVisibility(true, ntpPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @DisableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2,
        ChromeFeatureList.ENABLE_TOOLBAR_POSITIONING_IN_RESIZE_MODE
    })
    @Restriction(DeviceFormFactor.PHONE)
    // TODO(crbug.com/485814887): Clean up this test.
    public void testRender_incognitoNtpWithOmniboxAutofocus_toolbarTop() throws Exception {
        loadAndRenderIncognitoNtp("incognito_ntp_omnibox_autofocus_toolbar_top", true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP,
        ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2 + ":force_bottom_for_focused_omnibox/true"
    })
    @DisableFeatures(ChromeFeatureList.ENABLE_TOOLBAR_POSITIONING_IN_RESIZE_MODE)
    @Restriction(DeviceFormFactor.PHONE)
    // TODO(crbug.com/485814887): Clean up this test.
    public void testRender_incognitoNtpWithOmniboxAutofocus_toolbarBottom() throws Exception {
        loadAndRenderIncognitoNtp("incognito_ntp_omnibox_autofocus_toolbar_bottom", true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures({
        ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP,
        ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2
    })
    @Restriction(DeviceFormFactor.PHONE)
    public void testRender_incognitoNtp_keyboardOverlay_toolbarTop() throws Exception {
        loadAndRenderIncognitoNtp("incognito_ntp_keyboard_overlay_toolbar_top", false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2 + ":force_bottom_for_focused_omnibox/true")
    @DisableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void testRender_incognitoNtp_keyboardOverlay_toolbarBottom() throws Exception {
        loadAndRenderIncognitoNtp("incognito_ntp_keyboard_overlay_toolbar_bottom", false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
    @Restriction(DeviceFormFactor.PHONE)
    public void testRender_incognitoNtp_keyboardResize_toolbarTop() throws Exception {
        loadAndRenderIncognitoNtp("incognito_ntp_keyboard_resize_toolbar_top", true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP,
        ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2 + ":force_bottom_for_focused_omnibox/true"
    })
    @Restriction(DeviceFormFactor.PHONE)
    public void testRender_incognitoNtp_keyboardResize_toolbarBottom() throws Exception {
        loadAndRenderIncognitoNtp("incognito_ntp_keyboard_resize_toolbar_bottom", true);
    }

    private void loadAndRenderIncognitoNtp(String goldenId, boolean withOmniboxAutofocus)
            throws Exception {
        final Tab incognitoNtpTab =
                mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl(), true);

        if (!withOmniboxAutofocus) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mActivityTestRule
                                .getActivity()
                                .getToolbarManager()
                                .setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP);
                    });
        }

        verifyPhoneOmniboxFocusAndKeyboardVisibility(true, incognitoNtpTab);

        // Disable scrollbar and cursor to avoid screenshot diffs due to fading animation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View ntpScrollView =
                            mActivityTestRule.getActivity().findViewById(R.id.ntp_scrollview);
                    if (ntpScrollView != null) {
                        ntpScrollView.setVerticalScrollBarEnabled(false);
                    }

                    UrlBar urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
                    if (urlBar != null) {
                        urlBar.setCursorVisible(false);
                    }
                });

        View view = mActivityTestRule.getActivity().findViewById(android.R.id.content);
        mRenderTestRule.render(view, goldenId);
    }

    private void verifyPhoneOmniboxFocusAndKeyboardVisibility(boolean enabled, @Nullable Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            enabled
                                    ? "Omnibox should be focused."
                                    : "Omnibox should not be focused.",
                            mActivityTestRule.getActivity().getToolbarManager().isUrlBarFocused(),
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
                },
                VERIFY_FOCUS_MAX_TIME_TO_POLL_MS,
                VERIFY_FOCUS_POLLING_INTERVAL_MS);
    }

    private void verifyNonPhoneOmniboxFocusAndKeyboardVisibility(
            boolean enabled, CtaPageStation page) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            enabled
                                    ? "Omnibox should be focused."
                                    : "Omnibox should not be focused.",
                            page.getActivity().getToolbarManager().isUrlBarFocused(),
                            Matchers.is(enabled));

                    Tab tab = page.getTab();
                    if (tab != null && tab.getView() != null) {
                        Criteria.checkThat(
                                enabled
                                        ? "Keyboard should be visible."
                                        : "Keyboard should not be visible.",
                                KeyboardVisibilityDelegate.getInstance()
                                        .isKeyboardShowing(tab.getView()),
                                Matchers.is(enabled));
                    }
                },
                VERIFY_FOCUS_MAX_TIME_TO_POLL_MS,
                VERIFY_FOCUS_POLLING_INTERVAL_MS);
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
