// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.environment_integrity;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

/** Test suite for navigator.getEnvironmentIntegrity functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=WebEnvironmentIntegrity", "ignore-certificate-errors"})
@Batch(Batch.PER_CLASS)
public class EnvironmentIntegrityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_FILE = "/chrome/test/data/android/environment_integrity.html";
    private EmbeddedTestServer mTestServer;
    private String mUrl;
    private Tab mTab;
    private EnvironmentIntegrityUpdateWaiter mUpdateWaiter;

    /** Waits until the JavaScript code supplies a result. */
    private class EnvironmentIntegrityUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mStatus;

        public EnvironmentIntegrityUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = mActivityTestRule.getActivity().getActivityTab().getTitle();
            // Wait until the title indicates either success or failure.
            if (!title.startsWith("Success") && !title.startsWith("Fail")) return;
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
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = mActivityTestRule.getTestServer();
        mUrl = mTestServer.getURLWithHostName("test.host", TEST_FILE);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mUpdateWaiter = new EnvironmentIntegrityUpdateWaiter();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(mUpdateWaiter));
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTab.removeObserver(mUpdateWaiter));
    }

    /**
     * Verify that navigator.getEnvironmentIntegrity succeeds.
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    public void testGetEnvironmentIntegrity() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "doGetEnvironmentIntegrity('contentBinding')");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }
}
