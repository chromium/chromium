// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.components.infobars.InfoBar;

import java.util.concurrent.TimeoutException;

/** Tests for the appearance of InfoBars. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class InfoBarAppearanceTest {

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule public static ScreenShooter sScreenShooter = new ScreenShooter();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private InfoBarTestAnimationListener mListener;
    private Tab mTab;

    @Before
    public void setUp() throws InterruptedException {
        mListener = new InfoBarTestAnimationListener();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab = sActivityTestRule.getActivity().getActivityTab();
                    sActivityTestRule.getInfoBarContainer().addAnimationListener(mListener);
                });
    }

    @After
    public void tearDown() {
        InfoBarContainer container = sActivityTestRule.getInfoBarContainer();
        if (container != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        container.removeAnimationListener(mListener);
                        InfoBarContainer.removeInfoBarContainerForTesting(
                                sActivityTestRule.getActivity().getActivityTab());
                    });
        }
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testOomInfoBar() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> InfoBarContainer.get(mTab).addInfoBarForTesting(new NearOomInfoBar()));
        mListener.addInfoBarAnimationFinished("InfoBar was not added.");
        sScreenShooter.shoot("oom_infobar");
    }

    private void captureMiniAndRegularInfobar(InfoBar infobar) throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> InfoBarContainer.get(mTab).addInfoBarForTesting(infobar));
        mListener.addInfoBarAnimationFinished("InfoBar was not added.");
        sScreenShooter.shoot("compact");

        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);
        mListener.swapInfoBarAnimationFinished("InfoBar did not expand.");
        sScreenShooter.shoot("expanded");
    }
}
