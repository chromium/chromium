// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.containsString;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for HTTP headers sent to GAIA server when user is signed in. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninHeaderTest {
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    private String mGAIAUrl;

    private void launchTrustedWebActivity(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(PACKAGE_NAME, url);
        createSession(intent, PACKAGE_NAME);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();
        mEmbeddedTestServerRule.setServerUsesHttps(true);
        // Specify a Gaia url path.
        CommandLine.getInstance()
                .appendSwitchWithValue("gaia-url", mEmbeddedTestServerRule.getServer().getURL("/"));
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addTestAccountThenSignin();

        mGAIAUrl = mEmbeddedTestServerRule.getServer().getURL("/echoheader?X-Chrome-Connected");
    }

    @Test
    @MediumTest
    public void testXChromeConnectedHeader_In_TWA_ReturnsModeValueWithIncognitoOff()
            throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(mGAIAUrl);
        launchTrustedWebActivity(intent);
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String output =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.innerText");
        assertThat(output, containsString("mode=1,enable_account_consistency=true"));
    }

    @Test
    @MediumTest
    public void testXChromeConnectedHeader_In_CCT_ReturnsModeValueWithIncognitoOff()
            throws TimeoutException {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ContextUtils.getApplicationContext(), mGAIAUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String output =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.innerText");
        assertThat(output, containsString("mode=1,enable_account_consistency=true"));
    }

    @Test
    @MediumTest
    public void testXChromeConnectedHeader_InNonCCT_ReturnsModeWithIncognitoOn()
            throws TimeoutException {
        mChromeActivityTestRule.loadUrl(mGAIAUrl);
        Tab tab = mChromeActivityTestRule.getActivity().getActivityTab();
        String output =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.innerText");
        assertThat(output, containsString("mode=0,enable_account_consistency=true"));
    }
}
