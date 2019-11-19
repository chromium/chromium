// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static android.support.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.scrollToLastElement;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;

import java.util.concurrent.TimeoutException;

/**
 * Screenshot test for manual filling views. They ensure that we don't regress on visual details
 * like shadows, padding and RTL differences. Logic integration tests involving all filling
 * components belong into {@link ManualFillingIntegrationTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingUiCaptureTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final ScreenShooter mScreenShooter = new ScreenShooter();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessory", "LTR", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.cacheTestCredentials();
        mHelper.addGenerationButton();

        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBar");

        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswords");

        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolled");
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessory", "RTL", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswordsRTL()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(true);
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarRTL");

        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsRTL");

        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolledRTL");
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessoryModern", "LTR", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryV2WithPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        ManualFillingTestHelper.createAutofillTestProfiles();
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarV2");

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2");

        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2Scrolled");
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessoryModern", "RTL", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryV2WithPasswordsRTL()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(true);
        ManualFillingTestHelper.createAutofillTestProfiles();
        mHelper.cacheTestCredentials();
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.addGenerationButton();

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarV2RTL");

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2RTL");

        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2ScrolledRTL");
    }

    private void waitForUnrelatedChromeUi() throws InterruptedException {
        Thread.sleep(scaleTimeout(50)); // Reduces flakiness due to delayed events.
    }

    private void waitForActionsInAccessory() {
        whenDisplayed(withId(R.id.bar_items_view));
        onView(withId(R.id.bar_items_view)).check((view, noViewFound) -> {
            if (noViewFound != null) throw noViewFound;
            RecyclerViewTestUtils.waitForStableRecyclerView((RecyclerView) view);
        });
    }

    private void waitForSuggestionsInSheet() {
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        onView(withId(R.id.passwords_sheet)).check((view, noViewFound) -> {
            if (noViewFound != null) throw noViewFound;
            RecyclerViewTestUtils.waitForStableRecyclerView((RecyclerView) view);
        });
    }
}
