// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests starting the activity with URLs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MainActivityWithURLTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    /** Verify launch the activity with URL. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivityWithURL() {
        // Launch chrome
        mActivityTestRule.startOnTestServerUrl("/chrome/test/data/android/simple.html");
        String expectedTitle = "Activity test page";
        String title = ChromeTabUtils.getCurrentTabTitleOnUiThread(mActivityTestRule.getActivity());
        Assert.assertEquals(expectedTitle, title);
    }

    /** Launch and verify URL is neither null nor empty. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP) // https://crbug.com/1392547
    public void testLaunchActivity() {
        // Launch chrome
        mActivityTestRule.startFromLauncherAtNtp();
        String currentUrl =
                ChromeTabUtils.getCurrentTabUrlOnUiThread(mActivityTestRule.getActivity());
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());
    }

    /**
     * Launch a NTP and make sure it loads correctly. This makes sure the NTP loading complete
     * notification is received.
     */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testNewTabPageLaunch() {
        // Launch chrome with NTP.
        mActivityTestRule.startOnNtp();
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        String currentUrl = ChromeTabUtils.getCurrentTabUrlOnUiThread(activity);
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());

        // Open NTP.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), activity);

        currentUrl = ChromeTabUtils.getCurrentTabUrlOnUiThread(activity);
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());
    }
}
