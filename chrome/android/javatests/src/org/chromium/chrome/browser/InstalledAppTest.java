// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;

/** Test suite for navigator.getInstalledRelatedApps functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-blink-features=InstalledApp",
})
public class InstalledAppTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_FILE = "/content/test/data/android/installedapp.html";

    private EmbeddedTestServer mTestServer;

    private String mUrl;

    private Tab mTab;
    private InstalledAppUpdateWaiter mUpdateWaiter;

    /** Waits until the JavaScript code supplies a result. */
    private class InstalledAppUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mStatus;

        public InstalledAppUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = mActivityTestRule.getActivity().getActivityTab().getTitle();
            // Wait until the title indicates either success or failure.
            if (!title.startsWith("Success:") && !title.startsWith("Fail:")) return;
            mStatus = title;
            mCallbackHelper.notifyCalled();
        }

        public String waitForUpdate() throws Exception {
            mCallbackHelper.waitForCallback(0);
            return mStatus;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        mUrl = mTestServer.getURL(TEST_FILE);

        mTab = mActivityTestRule.getActivity().getActivityTab();
        mUpdateWaiter = new InstalledAppUpdateWaiter();
        mTab.addObserver(mUpdateWaiter);
    }

    @After
    public void tearDown() {
        mTab.removeObserver(mUpdateWaiter);
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Verify that InstalledApp succeeds.
     *
     * Note this isn't a very thorough test; it just expects an empty response. Testing any real
     * response would require setting up (or mocking) a real APK. There are extremely thorough
     * layout tests and Java unit tests for this feature. This end-to-end test just ensures that the
     * Mojo bridge between Blink and Java is working (regression: https://crbug.com/750348).
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"InstalledApp"})
    public void testGetInstalledRelatedApps() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("doGetInstalledRelatedApps()");
        Assert.assertEquals("Success: 0 related apps", mUpdateWaiter.waitForUpdate());
    }
}
