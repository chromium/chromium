// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.scrollToLastElement;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;

import java.util.concurrent.TimeoutException;

/**
 * Screenshot test for manual filling views. They ensure that we don't regress on visual details
 * like shadows, padding and RTL differences. Logic integration tests involving all filling
 * components belong into {@link ManualFillingIntegrationTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingUiCaptureTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

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
        mHelper.loadTestPage(false);
        ManualFillingTestHelper.createAutofillTestProfiles();
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBar");

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabAtPosition(0)));

        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswords");

        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolled");
    }

    @Test
    @MediumTest
    @Feature({"KeyboardAccessory", "RTL", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswordsRTL()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(true);
        ManualFillingTestHelper.createAutofillTestProfiles();
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarRTL");

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabAtPosition(0)));

        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsRTL");

        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolledRTL");
    }

    private void waitForUnrelatedChromeUi() throws InterruptedException {
        Thread.sleep(scaleTimeout(50)); // Reduces flakiness due to delayed events.
    }

    private void waitForActionsInAccessory() {
        whenDisplayed(withId(R.id.bar_items_view));
        RecyclerViewTestUtils.waitForStableRecyclerView(mHelper.getAccessoryBarView());
    }

    private void waitForSuggestionsInSheet() {
        whenDisplayed(withId(R.id.keyboard_accessory_sheet_frame));
        RecyclerViewTestUtils.waitForStableRecyclerView(
                mActivityTestRule.getActivity().findViewById(R.id.passwords_sheet));
    }
}
