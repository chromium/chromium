// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.scrollToAndSelectTab;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.scrollToLastElement;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/**
 * Screenshot test for manual filling views. They ensure that we don't regress on visual details
 * like shadows, padding and RTL differences. Logic integration tests involving all filling
 * components belong into {@link ManualFillingIntegrationTest}.
 */
// TODO(crbug.com/447076444): Enable Keyboard Accessory revamp flag
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP,
    ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_SUPPRESS_ACCESSORY_ON_EMPTY
})
@DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511286364
public class ManualFillingUiCaptureTest {
    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final ScreenShooter mScreenShooter = new ScreenShooter();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @MediumTest
    @Feature({"KeyboardAccessory", "LTR", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.startAtTestPage(/* isRtl= */ false);
        ManualFillingTestHelper.createAutofillTestProfiles();
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBar");

        scrollToAndSelectTab(R.string.password_accessory_sheet_toggle);

        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswords");

        waitForView(withId(R.id.passwords_sheet)).performViewAction(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolled");
    }

    @Test
    @MediumTest
    @Feature({"KeyboardAccessory", "RTL", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswordsRTL()
            throws InterruptedException, TimeoutException {
        mHelper.startAtTestPage(/* isRtl= */ true);
        ManualFillingTestHelper.createAutofillTestProfiles();
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarRTL");

        scrollToAndSelectTab(R.string.password_accessory_sheet_toggle);

        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsRTL");

        waitForView(withId(R.id.passwords_sheet)).performViewAction(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolledRTL");
    }

    private void waitForUnrelatedChromeUi() throws InterruptedException {
        Thread.sleep(scaleTimeout(50)); // Reduces flakiness due to delayed events.
    }

    private void waitForActionsInAccessory() {
        waitForView(withId(R.id.bar_items_view));
        RecyclerViewTestUtils.waitForStableRecyclerView(mHelper.getAccessoryBarView());
    }

    private void waitForSuggestionsInSheet() {
        waitForView(withId(R.id.keyboard_accessory_sheet_frame));
        RecyclerViewTestUtils.waitForStableRecyclerView(
                mActivityTestRule.getActivity().findViewById(R.id.passwords_sheet));
    }
}
