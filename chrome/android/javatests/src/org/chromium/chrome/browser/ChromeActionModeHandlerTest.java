// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests {@link ChromeActionModeHandler} operation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeActionModeHandlerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private void assertActionModeIsReady() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                WebContentsUtils.isActionModeSupported(
                                        mActivityTestRule.getWebContents())));
    }

    @Test
    @SmallTest
    public void testActionModeSetForNewTab() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        assertActionModeIsReady();

        LoadUrlParams urlParams = new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        // Assert that a new tab has an action mode callback set as expected.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTabProvider().get();
                    return mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    urlParams, TabLaunchType.FROM_LONGPRESS_FOREGROUND, tab, true);
                });
        assertActionModeIsReady();
    }
}
