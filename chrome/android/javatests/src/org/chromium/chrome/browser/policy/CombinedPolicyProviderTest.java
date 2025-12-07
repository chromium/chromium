// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.policy.PolicyProvider;

/** Instrumentation tests for {@link CombinedPolicyProvider} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CombinedPolicyProviderTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String DATA_URI = "data:text/plain;charset=utf-8;base64,dGVzdA==";
    private WebPageStation mPage;

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
    }

    /**
     * Checks that the {@link CombinedPolicyProvider} properly notifies tabs when incognito mode is
     * disabled.
     */
    @Test
    @Feature({"Policy"})
    @SmallTest
    public void testTerminateIncognitoSon() {
        IncognitoNewTabPageStation incognitoNtp = mPage.openNewIncognitoTabOrWindowFast();
        TabModel incognitoTabModel = incognitoNtp.getTabModel();
        WebPageStation webPage = incognitoNtp.loadWebPageProgrammatically(DATA_URI);
        webPage.openFakeLinkToWebPage(DATA_URI);
        Assert.assertEquals(2, getTabCountOnUiThread(incognitoTabModel));

        final CombinedPolicyProvider provider = CombinedPolicyProvider.get();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        provider.registerProvider(
                                new PolicyProvider() {
                                    @Override
                                    public void refresh() {
                                        terminateIncognitoSession();
                                    }
                                }));

        Assert.assertEquals(0, getTabCountOnUiThread(incognitoTabModel));
    }
}
