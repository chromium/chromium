// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.webapk.lib.client.WebApkValidator;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkIntegrationTest {
    public final ChromeActivityTestRule<WebApkActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(WebApkActivity.class);

    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(mNativeLibraryTestRule, 0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
                                          .around(mActivityTestRule)
                                          .around(mNativeLibraryTestRule)
                                          .around(mCertVerifierRule);

    /** Returns URL for the passed-in host which maps to a page on the EmbeddedTestServer. */
    private String getUrlForHost(String host) {
        return "https://" + host + "/defaultresponse";
    }

    @Before
    public void setUp() {
        WebApkUpdateManager.setUpdatesEnabledForTesting(false);
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri =
                Uri.parse(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/"));
        CommandLine.getInstance().appendSwitchWithValue(
                ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
        WebApkValidator.disableValidationForTesting();
    }

    /**
     * Tests that WebApkActivities are started properly by WebappLauncherActivity.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testWebApkLaunchesByLauncherActivity() {
        String pwaRocksUrl = getUrlForHost("pwa.rocks");

        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(InstrumentationRegistry.getTargetContext().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebApkConstants.EXTRA_URL, pwaRocksUrl)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, "org.chromium.webapk.test");

        mActivityTestRule.startActivityCompletely(intent);

        WebApkActivity lastActivity = mActivityTestRule.getActivity();
        Assert.assertEquals(pwaRocksUrl, lastActivity.getWebappInfo().url());
    }
}
