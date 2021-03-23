// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;

import java.util.concurrent.TimeoutException;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkIntegrationTest {
    public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
                                          .around(mActivityTestRule)
                                          .around(mCertVerifierRule);

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri =
                Uri.parse(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/"));
        CommandLine.getInstance().appendSwitchWithValue(
                ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
        WebApkValidator.setDisableValidationForTesting(true);
    }

    /**
     * Tests that sending deep link intent to WebAPK launches WebAPK Activity.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testDeepLink() {
        String pageUrl = "https://pwa-directory.appspot.com/defaultresponse";

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(pageUrl));
        intent.setPackage("org.chromium.webapk.test");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        InstrumentationRegistry.getTargetContext().startActivity(intent);

        WebappActivity lastActivity = ChromeActivityTestRule.waitFor(WebappActivity.class);
        Assert.assertEquals(ActivityType.WEB_APK, lastActivity.getActivityType());
        Assert.assertEquals(pageUrl, lastActivity.getIntentDataProvider().getUrlToLoad());
    }

    /**
     * Tests launching WebAPK via POST share intent.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testShare() throws TimeoutException {
        final String sharedSubject = "Fun tea parties";
        final String sharedText = "Boston";
        final String expectedShareUrl = "https://pwa-directory.appspot.com/echoall";

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setPackage("org.chromium.webapk.test");
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_SUBJECT, sharedSubject);
        intent.putExtra(Intent.EXTRA_TEXT, sharedText);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        InstrumentationRegistry.getTargetContext().startActivity(intent);

        WebappActivity lastActivity = ChromeActivityTestRule.waitFor(WebappActivity.class);
        Assert.assertEquals(ActivityType.WEB_APK, lastActivity.getActivityType());

        Tab tab = lastActivity.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, expectedShareUrl);
        String postDataJson = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.getElementsByTagName('pre')[0].innerText");
        assertEquals("\"title=Fun+tea+parties\\ntext=Boston\\n\"", postDataJson);
    }
}
