// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.view.KeyEvent;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests system and application interaction with Contextual Search using instrumentation tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchSystemTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/simple_test.html";
        super.setUp();
    }

    // ============================================================================================
    // App Menu suppression support
    // ============================================================================================

    /** Simulates pressing the App Menu button. */
    private void pressAppMenuKey() {
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_MENU);
    }

    /** Simulates pressing back button. */
    private void pressBackButton() {
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
    }

    private void closeAppMenu() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().hideAppMenu());
    }

    /** Asserts whether the App Menu is visible. */
    private void assertAppMenuVisibility(final boolean isVisible) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            sActivityTestRule
                                    .getAppMenuCoordinator()
                                    .getAppMenuHandler()
                                    .isAppMenuShowing(),
                            Matchers.is(isVisible));
                });
    }

    // ============================================================================================
    // Tab Crash
    // ============================================================================================

    /** Tests that the panel closes when its base page crashes. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled in 2018.  See https://crbug.com/832539.
    public void testContextualSearchDismissedOnForegroundTabCrash() throws Exception {
        triggerResolve(SEARCH_NODE);
        Assert.assertEquals(SEARCH_NODE_TERM, getSelectedText());
        waitForPanelToPeek();

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    ChromeTabUtils.simulateRendererKilledForTesting(
                            sActivityTestRule.getActivity().getActivityTab());
                });

        // Give the panelState time to change
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mPanel.getPanelState(), Matchers.not(PanelState.PEEKED));
                });

        assertPanelClosedOrUndefined();
    }

    /** Test the the panel does not close when some background tab crashes. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Revived 6/2022 based on reviver: https://crbug.com/1333277
    // Previously disabled: https://crbug.com/1192285, https://crbug.com/1192561
    public void testContextualSearchNotDismissedOnBackgroundTabCrash() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        final Tab tab2 =
                TabModelUtils.getCurrentTab(sActivityTestRule.getActivity().getCurrentTabModel());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.setIndex(sActivityTestRule.getActivity().getCurrentTabModel(), 0);
                });

        triggerResolve(SEARCH_NODE);
        Assert.assertEquals(SEARCH_NODE_TERM, getSelectedText());
        waitForPanelToPeek();

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    ChromeTabUtils.simulateRendererKilledForTesting(tab2);
                });

        waitForPanelToPeek();
    }

    // ============================================================================================
    // App Menu Suppression
    // ============================================================================================

    /** Tests that the App Menu gets suppressed when Search Panel is expanded. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testAppMenuSuppressedWhenExpanded() throws Exception {
        triggerPanelPeek();
        expandPanelAndAssert();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        closePanel();

        pressAppMenuKey();
        assertAppMenuVisibility(true);

        closeAppMenu();
    }

    /** Tests that the App Menu gets suppressed when Search Panel is maximized. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAppMenuSuppressedWhenMaximized() throws Exception {
        triggerPanelPeek();
        maximizePanel();
        waitForPanelToMaximize();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        pressBackButton();
        waitForPanelToClose();

        pressAppMenuKey();
        assertAppMenuVisibility(true);

        closeAppMenu();
    }
}
