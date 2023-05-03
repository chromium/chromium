// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.content.pm.ActivityInfo;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.TranslateCompactInfoBar;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.TranslateUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.infobars.InfoBar;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the translate infobar, assumes it runs on a system with language
 * preferences set to English.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(TranslateAssistContentTest.TRANSLATE_BATCH_NAME)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TranslateCompactInfoBarTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NON_TRANSLATE_PAGE = "/chrome/test/data/android/simple.html";

    private InfoBarContainer mInfoBarContainer;
    private InfoBarTestAnimationListener mListener;

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TranslateBridge.setIgnoreMissingKeyForTesting(true);
            mInfoBarContainer = sActivityTestRule.getInfoBarContainer();
            mListener = new InfoBarTestAnimationListener();
            mInfoBarContainer.addAnimationListener(mListener);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mInfoBarContainer.removeAnimationListener(mListener));
    }

    /**
     * Returns true if a test that requires internet access should be skipped due to an
     * out-of-process NetworkService. When the NetworkService is run out-of-process, a fake DNS
     * resolver is used that will fail to resolve any non-local names. crbug.com/1134812 is tracking
     * the changes to make the translate service mockable and remove the internet requirement.
     */
    private boolean shouldSkipDueToNetworkService() {
        return !ChromeFeatureList.isEnabled("NetworkServiceInProcess2");
    }

    /**
     * Test that the new translate compact UI appears and has at least 2 tabs.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction({Restriction.RESTRICTION_TYPE_INTERNET})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI, ChromeFeatureList.TRANSLATE_TFLITE})
    public void testTranslateCompactInfoBarAppears() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        InfoBar infoBar = mInfoBarContainer.getInfoBarsForTesting().get(0);
        TranslateUtil.assertCompactTranslateInfoBar(infoBar);
        TranslateUtil.assertHasAtLeastTwoLanguageTabs((TranslateCompactInfoBar) infoBar);
    }

    /**
     * Test the overflow menus of new translate compact UI.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction({Restriction.RESTRICTION_TYPE_INTERNET})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI, ChromeFeatureList.TRANSLATE_TFLITE})
    public void testTranslateCompactInfoBarOverflowMenus() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        TranslateCompactInfoBar infoBar =
                (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);
        TranslateUtil.hasMenuButton(infoBar);

        // 1. Click on menu button and make sure overflow menu appears
        TranslateUtil.clickMenuButtonAndAssertMenuShown(infoBar);

        // 2. Click on "More language" in the overflow menu and make sure language menu appears
        TranslateUtil.clickMoreLanguageButtonAndAssertLanguageMenuShown(
                InstrumentationRegistry.getInstrumentation(), infoBar);
    }

    /**
     * Tests that the overflow menu is dismissed when the orientation changes.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction({Restriction.RESTRICTION_TYPE_INTERNET})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI, ChromeFeatureList.TRANSLATE_TFLITE})
    public void testTabMenuDismissedOnOrientationChange() throws Exception {
        if (shouldSkipDueToNetworkService()) return;
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        TranslateCompactInfoBar infoBar =
                (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);
        TranslateUtil.hasMenuButton(infoBar);
        TranslateUtil.clickMenuButtonAndAssertMenuShown(infoBar);

        // 1. Set orientation to portrait
        sActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 2. Check if overflow menu is dismissed
        Assert.assertFalse(infoBar.isShowingLanguageMenuForTesting());

        // 3. Reset orientation
        sActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Test tab focus is correct, even after closing and (manually) re-opening infobar.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction({Restriction.RESTRICTION_TYPE_INTERNET})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI, ChromeFeatureList.TRANSLATE_TFLITE})
    public void testTranslateCompactInfoBarReopenOnTarget() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE));

        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        TranslateCompactInfoBar infoBar =
                (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);
        // Translate.
        TranslateUtil.clickTargetMenuItem(infoBar, "en");
        // Close bar.
        InfoBarUtil.clickCloseButton(infoBar);

        // Invoke bar by clicking the manual translate button.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), R.id.translate_id);

        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }

    /**
     * Test that translation starts automatically when "Translate..." is pressed in the menu.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction({Restriction.RESTRICTION_TYPE_INTERNET})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testStartTranslateOnManualInitiation() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that won't trigger the translate recommendation.
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE));

        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        // Invoke bar by clicking the manual translate button.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), R.id.translate_id);

        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }

    /**
     * Test that pressing "Translate..." will start a translation even if the infobar is visible.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction({Restriction.RESTRICTION_TYPE_INTERNET})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI, ChromeFeatureList.TRANSLATE_TFLITE})
    public void testManualInitiationWithBarOpen() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE));

        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), R.id.translate_id);

        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);

        // Verify that hitting "Translate..." again doesn't revert the translation.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), R.id.translate_id);

        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }
}
