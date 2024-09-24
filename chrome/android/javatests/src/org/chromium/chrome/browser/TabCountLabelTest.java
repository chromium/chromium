// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.widget.ImageButton;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Test suite for the tab count widget on the phone toolbar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabCountLabelTest {
    /** Check the tabCount string against an expected value. */
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private void tabCountLabelCheck(String stepName, String labelExpected) {
        ImageButton tabSwitcherBtn =
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        TabSwitcherDrawable drawable = (TabSwitcherDrawable) tabSwitcherBtn.getDrawable();
        String labelFromDrawable = drawable.getTextRenderedForTesting();
        Assert.assertEquals(
                stepName
                        + ", "
                        + labelExpected
                        + " tab[s] expected, label shows "
                        + labelFromDrawable,
                labelExpected,
                labelFromDrawable);
    }

    /** Verify displayed Tab Count matches the actual number of tabs. */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testTabCountLabel() {
        tabCountLabelCheck("Initial state", "1");
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        // Make sure the TAB_CREATED notification went through
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        tabCountLabelCheck("After new tab", "2");
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        // Make sure the TAB_CLOSED notification went through
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        tabCountLabelCheck("After close tab", "1");
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
