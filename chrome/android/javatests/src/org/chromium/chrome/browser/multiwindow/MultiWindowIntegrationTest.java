// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.moveActivityToFront;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForTabs;

import android.os.Build.VERSION_CODES;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Integration testing for Android's N+ MultiWindow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MultiWindowIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws InterruptedException {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature("MultiWindow")
    @DisabledTest(message = "Flaky on test-n-phone https://crbug/1197125")
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    public void testIncognitoNtpHandledCorrectly() {
        try {
            ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

            mActivityTestRule.newIncognitoTabFromMenu();
            Assert.assertTrue(mActivityTestRule.getActivity().getActivityTab().isIncognito());
            final int incognitoTabId = mActivityTestRule.getActivity().getActivityTab().getId();

            MenuUtils.invokeCustomMenuActionSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(),
                    R.id.move_to_other_window_menu_id);

            final ChromeTabbedActivity2 cta2 = waitForSecondChromeTabbedActivity();

            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                cta2.getTabModelSelector().getModel(true).getCount(),
                                Matchers.is(1));
                    });

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertEquals(
                                1, TabWindowManagerSingleton.getInstance().getIncognitoTabCount());

                        // Ensure the same tab exists in the new activity.
                        Assert.assertEquals(incognitoTabId, cta2.getActivityTab().getId());
                    });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        }
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338976206
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.S_V2) // https://crbug.com/1297370
    @Feature("MultiWindow")
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING,
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
    })
    public void testMoveTabTwice() {
        // Load 'google' in separate tab.
        int googleTabId =
                mActivityTestRule
                        .loadUrlInNewTab(
                                mTestServer.getURL("/chrome/test/data/android/google.html"))
                        .getId();

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // Move 'google' tab to cta2.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                cta,
                R.id.move_to_other_window_menu_id);

        final ChromeTabbedActivity2 cta2 = waitForSecondChromeTabbedActivity();

        // At this point cta should have NTP tab, and cta2 should have 'google' tab.
        waitForTabs("CTA", cta, 1, Tab.INVALID_TAB_ID);
        waitForTabs("CTA2", cta2, 1, googleTabId);

        // Move 'google' tab back to cta.
        moveActivityToFront(cta2);
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                cta2,
                R.id.move_to_other_window_menu_id);

        // At this point cta2 should have zero tabs, and cta should have 2 tabs (NTP, 'google').
        waitForTabs("CTA2", cta2, 0, Tab.INVALID_TAB_ID);
        waitForTabs("CTA", cta, 2, googleTabId);
    }

    @Test
    @MediumTest
    @Feature("MultiWindow")
    @DisabledTest(message = "Flaky on test-n-phone https://crbug/1197125")
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING,
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
    })
    // TODO(crbug.com/40822813): Enable this test for tablet once the tab switcher is supported.
    @Restriction(DeviceFormFactor.PHONE)
    public void testMovingLastTabKeepsActivityAlive() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        int blankTab = cta.getActivityTabProvider().get().getId();

        // Move the blank tab to cta2.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                cta,
                R.id.move_to_other_window_menu_id);

        final ChromeTabbedActivity2 cta2 = waitForSecondChromeTabbedActivity();

        // At this point cta2 should have zero tabs, and cta should have 1 tab.
        waitForTabs("CTA", cta, 0, Tab.INVALID_TAB_ID);
        waitForTabs("CTA2", cta2, 1, blankTab);

        // Once all the tabs from one activity have been removed, the tab switcher should be shown.
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        // The activity should also remain alive.
        Assert.assertFalse("The original activity should not be finishing!", cta.isFinishing());
        Assert.assertFalse("The original activity should still be alive!", cta.isDestroyed());
    }
}
