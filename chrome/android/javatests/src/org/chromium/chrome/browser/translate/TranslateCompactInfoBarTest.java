// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.content.pm.ActivityInfo;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.TranslateCompactInfoBar;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.TranslateUtil;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the translate infobar, assumes it runs on a system with language
 * preferences set to English.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TranslateCompactInfoBarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";

    private InfoBarContainer mInfoBarContainer;
    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mInfoBarContainer = mActivityTestRule.getInfoBarContainer();
        mListener = new InfoBarTestAnimationListener();
        mInfoBarContainer.addAnimationListener(mListener);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Test that the new translate compact UI appears and has at least 2 tabs.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTranslateCompactInfoBarAppears() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
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
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTranslateCompactInfoBarOverflowMenus() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
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
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTabMenuDismissedOnOrientationChange() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        TranslateCompactInfoBar infoBar =
                (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);
        TranslateUtil.hasMenuButton(infoBar);
        TranslateUtil.clickMenuButtonAndAssertMenuShown(infoBar);

        // 1. Set orientation to portrait
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 2. Check if overflow menu is dismissed
        Assert.assertFalse(infoBar.isShowingLanguageMenuForTesting());

        // 3. Reset orientation
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Test tab focus is correct, even after closing and (manually) re-opening infobar.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
    public void testTranslateCompactInfoBarReopenOnTarget() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(TRANSLATE_PAGE));
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");

        TranslateCompactInfoBar infoBar =
                (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);

        // Only the source tab is selected.
        Assert.assertTrue(infoBar.isSourceTabSelectedForTesting());
        Assert.assertFalse(infoBar.isTargetTabSelectedForTesting());

        // Translate.
        TranslateUtil.clickTargetMenuItem(infoBar, "en");
        // Close bar.
        InfoBarUtil.clickCloseButton(infoBar);

        // Invoke bar by clicking the manual translate button.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.translate_id);

        infoBar = (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);

        // Only the target tab is selected.
        Assert.assertFalse(infoBar.isSourceTabSelectedForTesting());
        Assert.assertTrue(infoBar.isTargetTabSelectedForTesting());
    }
}
