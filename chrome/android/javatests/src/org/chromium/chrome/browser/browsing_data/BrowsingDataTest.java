// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static org.chromium.base.test.util.Matchers.is;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Integration tests for browsing data deletion. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Batch(Batch.PER_CLASS)
public class BrowsingDataTest {
    private static final String TEST_FILE = "/content/test/data/browsing_data/site_data.html";

    private EmbeddedTestServer mTestServer;
    private String mUrl;

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Before
    public void setUp() throws Exception {
        mTestServer = sActivityTestRule.getTestServer();
        mUrl = mTestServer.getURL(TEST_FILE);
    }

    private void clearBrowsingData(int dataType, int timePeriod) throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled, new int[] {dataType}, timePeriod);
                });
        helper.waitForCallback(0);
    }

    private int getCookieCount() throws Exception {
        String[] out = {""};
        BrowsingDataCounterBridge[] counter = {null};
        CallbackHelper helper = new CallbackHelper();
        BrowsingDataCounterBridge.BrowsingDataCounterCallback callback =
                (result) -> {
                    if (result.equals("Calculating…")) return;
                    out[0] = result;
                    helper.notifyCalled();
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    counter[0] =
                            new BrowsingDataCounterBridge(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    callback,
                                    BrowsingDataType.SITE_DATA,
                                    ClearBrowsingDataTab.ADVANCED);
                });
        helper.waitForCallback(0);
        // The counter returns a result like "3 sites" or "None".
        if (out[0].equals("None")) return 0;
        String cookieCount = out[0].replaceAll("[^0-9]", "");
        Assert.assertFalse("Result should contain a number: " + out[0], cookieCount.isEmpty());
        return Integer.parseInt(cookieCount);
    }

    private String runJavascriptAsync(String type) throws Exception {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                sActivityTestRule.getWebContents(), type);
    }

    private String runJavascriptSync(String type) throws Exception {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                sActivityTestRule.getWebContents(), type);
    }

    /** Test cookies deletion. */
    @Test
    @SmallTest
    public void testCookiesDeleted() throws Exception {
        Assert.assertEquals(0, getCookieCount());
        sActivityTestRule.loadUrl(mUrl);
        Assert.assertEquals("false", runJavascriptSync("hasCookie()"));

        runJavascriptSync("setCookie()");
        Assert.assertEquals("true", runJavascriptSync("hasCookie()"));
        Assert.assertEquals(1, getCookieCount());

        clearBrowsingData(BrowsingDataType.SITE_DATA, TimePeriod.LAST_HOUR);
        Assert.assertEquals("false", runJavascriptSync("hasCookie()"));
        Assert.assertEquals(0, getCookieCount());
    }

    /** Test site data deletion. */
    @Test
    @SmallTest
    public void testSiteDataDeleted() throws Exception {
        // TODO(dullweber): Investigate, why WebSql fails this test.
        List<String> siteData =
                Arrays.asList(
                        "LocalStorage",
                        "ServiceWorker",
                        "CacheStorage",
                        "FileSystem",
                        "IndexedDb" /*, "WebSql"*/);
        sActivityTestRule.loadUrl(mUrl);

        for (String type : siteData) {
            Assert.assertEquals(type, 0, getCookieCount());
            Assert.assertEquals(type, "false", runJavascriptAsync("has" + type + "Async()"));

            runJavascriptAsync("set" + type + "Async()");
            Assert.assertEquals(type, 1, getCookieCount());
            Assert.assertEquals(type, "true", runJavascriptAsync("has" + type + "Async()"));

            clearBrowsingData(BrowsingDataType.SITE_DATA, TimePeriod.LAST_HOUR);
            Assert.assertEquals(type, 0, getCookieCount());
            Assert.assertEquals(type, "false", runJavascriptAsync("has" + type + "Async()"));

            // Some types create data by checking for them, so we need to do a cleanup at the end.
            clearBrowsingData(BrowsingDataType.SITE_DATA, TimePeriod.LAST_HOUR);
        }
    }

    /**
     * Test all data deletion for incognito profile. This only checks to see if an android specific
     * code crashes or not. For details see, crbug.com/990624.
     */
    @Test
    @SmallTest
    public void testAllDataDeletedForIncognito() throws Exception {
        // TODO(roagarwal) : Crashes on BrowsingDataType.SITE_SETTINGS, BrowsingDataType.BOOKMARKS
        // data types.
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingDataIncognitoForTesting(
                                    helper::notifyCalled,
                                    new int[] {
                                        BrowsingDataType.HISTORY,
                                        BrowsingDataType.CACHE,
                                        BrowsingDataType.SITE_DATA,
                                        BrowsingDataType.PASSWORDS,
                                        BrowsingDataType.FORM_DATA
                                    },
                                    TimePeriod.LAST_HOUR);
                });
        helper.waitForCallback(0);
    }

    /** Test that both local and account passwords are deleted. */
    @Test
    @SmallTest
    @Restriction({GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15})
    @RequiresRestart("crbug.com/358427311")
    public void testLocalAndAccountPasswordsDeleted() throws Exception {
        // Set up a syncing user with one password in each store.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        PasswordManagerTestHelper.setAccountForPasswordStore(SigninTestRule.TEST_ACCOUNT_EMAIL);
        PasswordStoreBridge bridge =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new PasswordStoreBridge(sActivityTestRule.getProfile(false)));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bridge.insertPasswordCredentialInProfileStore(
                            new PasswordStoreCredential(
                                    new GURL("https://site1.com"), "user1", "pwd1"));
                    bridge.insertPasswordCredentialInAccountStore(
                            new PasswordStoreCredential(
                                    new GURL("https://site2.com"), "user2", "pwd2"));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "The profile store should've had one password",
                            bridge.getPasswordStoreCredentialsCountForProfileStore(),
                            is(1));
                    Criteria.checkThat(
                            "The account store should've had one password",
                            bridge.getPasswordStoreCredentialsCountForAccountStore(),
                            is(1));
                });

        clearBrowsingData(BrowsingDataType.PASSWORDS, TimePeriod.ALL_TIME);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "The profile store should be empty",
                            bridge.getPasswordStoreCredentialsCountForProfileStore(),
                            is(0));
                    Criteria.checkThat(
                            "The account store should be empty",
                            bridge.getPasswordStoreCredentialsCountForAccountStore(),
                            is(0));
                });
    }

    /** Test history deletion. */
    @Test
    @SmallTest
    public void testHistoryDeleted() throws Exception {
        Assert.assertEquals(0, getCookieCount());
        sActivityTestRule.loadUrlInNewTab(mUrl);
        Assert.assertEquals("false", runJavascriptSync("hasHistory()"));

        runJavascriptSync("setHistory()");
        Assert.assertEquals("true", runJavascriptSync("hasHistory()"));

        clearBrowsingData(BrowsingDataType.HISTORY, TimePeriod.LAST_HOUR);
        Assert.assertEquals("false", runJavascriptSync("hasHistory()"));
    }
}
