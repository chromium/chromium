// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.core.app.ApplicationProvider;
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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests starting the activity with URLs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MainActivityWithURLTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /** Verify launch the activity with URL. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivityWithURL() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        // Launch chrome
        mActivityTestRule.startMainActivityWithURL(
                testServer.getURL("/chrome/test/data/android/simple.html"));
        String expectedTitle = "Activity test page";
        TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        String title = ChromeTabUtils.getTitleOnUiThread(model.getTabAt(model.index()));
        Assert.assertEquals(expectedTitle, title);
    }

    /** Launch and verify URL is neither null nor empty. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    @Restriction(DeviceFormFactor.TABLET) // https://crbug.com/1392547
    public void testLaunchActivity() {
        // Launch chrome
        mActivityTestRule.startMainActivityFromLauncher();
        String currentUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab());
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
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        String currentUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab());
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());

        // Open NTP.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        currentUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab());
        Assert.assertNotNull(currentUrl);
        Assert.assertEquals(false, currentUrl.isEmpty());
    }
}
