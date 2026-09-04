// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Public Transit tests for operations through the app menu in multi-window. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Batching not yet supported in multi-window")
// In phones, the New Window option in the app menu is only enabled when already in multi-window or
// multi-display mode with Chrome not running in an adjacent window.
@Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
public class MultiWindowAppMenuTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288091
    public void testOpenNewWindow_fromWebPage() {
        doTestOpenNewWindow();
    }

    private void doTestOpenNewWindow() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);
        TransitAsserts.assertFinalDestinations(pageInFirstWindow, pageInSecondWindow);
    }

    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // https://crbug.com/500749408
    public void testOpenNewWindow_fromIncognitoNtp() {
        doTestOpenNewWindow_fromIncognitoNtp();
    }

    private void doTestOpenNewWindow_fromIncognitoNtp() {
        WebPageStation blankPage = mCtaTestRule.startOnIncognitoBlankPage();
        IncognitoNewTabPageStation pageInFirstWindow = blankPage.openNewIncognitoTabFast();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);
        TransitAsserts.assertFinalDestinations(pageInFirstWindow, pageInSecondWindow);
    }

    @Test
    @LargeTest
    public void testOpenAndCloseNewWindow() {
        doTestOpenAndCloseNewWindow();
    }

    private void doTestOpenAndCloseNewWindow() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);

        pageInSecondWindow.finishActivity();

        TransitAsserts.assertFinalDestinations(pageInFirstWindow);
    }

    @Test
    @LargeTest
    public void testOpenNewWindowAndCloseOriginal() {
        doTestOpenNewWindowAndCloseOriginal();
    }

    private void doTestOpenNewWindowAndCloseOriginal() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);

        pageInFirstWindow.finishActivity();

        TransitAsserts.assertFinalDestinations(pageInSecondWindow);
    }

    @Test
    @LargeTest
    @DisableFeatures({
        ChromeFeatureList.SETTINGS_MULTI_COLUMN,
        ChromeFeatureList.SETTINGS_IN_TAB, // crbug.com/521895796
        ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP // crbug.com/556881398
    })
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288091
    public void testInteractWithBothWindows() {
        doTestInteractWithBothWindows();
    }

    private void doTestInteractWithBothWindows() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        pageInFirstWindow.openRegularTabAppMenu().openSettings();
        pageInSecondWindow.openRegularTabSwitcher();
    }
}
