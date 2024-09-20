// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;

import androidx.browser.auth.AuthTabIntent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Instrumentation test for external navigation handling of a Custom Tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabExternalNavigationTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mTestAppActivityTestRule =
            new ChromeTabbedActivityTestRule();

    /** A dummy activity that claims to handle "customtab://customtabtest". */
    public static class DummyActivityForSpecialScheme extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    /** A dummy activity that claims to handle "http://customtabtest.com". */
    public static class DummyActivityForHttp extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    private static final String TWA_PACKAGE_NAME = "com.foo.bar";
    private static final String TEST_PATH = "/chrome/test/data/android/google.html";
    private static final String CUSTOM_SCHEME = "myscheme";
    private static final String AUTH_TAB_CUSTOM_SCHEME_REDIRECT_URL = "myscheme://auth?token=clank";
    private static final String AUTH_TAB_HTTPS_REDIRECT_HOST = "www.clank.com";
    private static final String AUTH_TAB_HTTPS_REDIRECT_PATH = "/auth?token=clank";
    private static final String AUTH_TAB_HTTPS_REDIRECT_URL =
            UrlConstants.HTTPS_URL_PREFIX
                    + AUTH_TAB_HTTPS_REDIRECT_HOST
                    + AUTH_TAB_HTTPS_REDIRECT_PATH;
    private static final String AUTH_TAB_OTHER_URL = "https://www.clank.com/auth/login-fail.html";
    private CustomTabNavigationDelegate mNavigationDelegate;
    private EmbeddedTestServer mTestServer;
    private ExternalNavigationHandler mUrlHandler;
    private CustomTabActivity mAuthTab;

    @Before
    public void setUp() throws Exception {
        mCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = mCustomTabActivityTestRule.getTestServer();
    }

    private void setUpTwa() throws TimeoutException {
        launchTwa(TWA_PACKAGE_NAME, mTestServer.getURL(TEST_PATH));
        finishSetUp(mCustomTabActivityTestRule.getActivity());
    }

    private void setUpAuthTab() throws TimeoutException {
        launchAuthTab(mTestServer.getURL(TEST_PATH));
        finishSetUp(mAuthTab);
    }

    private void finishSetUp(CustomTabActivity activity) {
        Tab tab = activity.getActivityTab();
        TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(tab);
        assertTrue(delegateFactory instanceof CustomTabDelegateFactory);
        CustomTabDelegateFactory customTabDelegateFactory =
                ((CustomTabDelegateFactory) delegateFactory);
        mUrlHandler =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> customTabDelegateFactory.createExternalNavigationHandler(tab));
        assertTrue(
                customTabDelegateFactory.getExternalNavigationDelegate()
                        instanceof CustomTabNavigationDelegate);
        mNavigationDelegate =
                (CustomTabNavigationDelegate)
                        customTabDelegateFactory.getExternalNavigationDelegate();
    }

    private void launchTwa(String twaPackageName, String url) throws TimeoutException {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(twaPackageName, url);
        TrustedWebActivityTestUtil.createSession(intent, twaPackageName);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    private void launchAuthTab(String url) throws TimeoutException {
        mTestAppActivityTestRule.startMainActivityOnBlankPage();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                CustomTabsIntentTestUtils.createCustomTabIntent(context, url, false, builder -> {})
                        .putExtra(AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, true)
                        .putExtra(AuthTabIntent.EXTRA_REDIRECT_SCHEME, CUSTOM_SCHEME)
                        .putExtra(
                                AuthTabIntentDataProvider.EXTRA_HTTPS_REDIRECT_HOST,
                                AUTH_TAB_HTTPS_REDIRECT_HOST)
                        .putExtra(
                                AuthTabIntentDataProvider.EXTRA_HTTPS_REDIRECT_PATH,
                                AUTH_TAB_HTTPS_REDIRECT_PATH);
        String packageName = context.getPackageName();
        TrustedWebActivityTestUtil.spoofVerification(packageName, AUTH_TAB_HTTPS_REDIRECT_URL);
        // TODO(b/358167556): Support #startActivityForResult in TestRule
        mAuthTab =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> {
                            mTestAppActivityTestRule
                                    .getActivity()
                                    .startActivityForResult(intent, 0);
                        });
        ChromeActivityTestRule.waitForActivityNativeInitializationComplete(mAuthTab);
        ChromeActivityTestRule.waitForDeferredStartup(mAuthTab);
    }

    private OverrideUrlLoadingResult getOverrideUrlLoadingResult(String url) {
        ExternalNavigationHandler.sAllowIntentsToSelfForTesting = true;
        final GURL testUrl = new GURL(url);
        RedirectHandler redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0, false, true);
        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(testUrl, false)
                        .setIsMainFrame(true)
                        .setIsRendererInitiated(true)
                        .setRedirectHandler(redirectHandler)
                        .build();
        return mUrlHandler.shouldOverrideUrlLoading(params);
    }

    /**
     * For urls with special schemes and hosts, and there is exactly one activity having a matching
     * intent filter, the framework will make that activity the default handler of the special url.
     * This test tests whether chrome is able to start the default external handler.
     */
    @Test
    @SmallTest
    public void testExternalActivityStartedForDefaultUrl() throws TimeoutException {
        setUpTwa();
        OverrideUrlLoadingResult result =
                getOverrideUrlLoadingResult("customtab://customtabtest/intent");
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, result.getResultType());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_AUTH_TAB)
    public void testAuthTabShouldReturnAsActivityResult_customScheme() throws TimeoutException {
        setUpAuthTab();

        var result = getOverrideUrlLoadingResult(AUTH_TAB_CUSTOM_SCHEME_REDIRECT_URL);

        // AuthTab does not launch an external intent for a custom scheme URL, but passes
        // the result back to the calling app and closes itself.
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_CLOSING_AFTER_AUTH, result.getResultType());
        Assert.assertTrue(mAuthTab.isFinishing());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_AUTH_TAB)
    public void testAuthTabReturnAsActivityResult_httpsRedirectUrl() throws TimeoutException {
        setUpAuthTab();
        var result = getOverrideUrlLoadingResult(AUTH_TAB_OTHER_URL);
        Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, result.getResultType());
        Assert.assertFalse("AuthTab should keep running", mAuthTab.isFinishing());

        result = getOverrideUrlLoadingResult(AUTH_TAB_HTTPS_REDIRECT_URL);
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_CLOSING_AFTER_AUTH, result.getResultType());
        Assert.assertTrue("AuthTab should be closed", mAuthTab.isFinishing());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_AUTH_TAB)
    public void testAuthTabReturnAsActivityResult_httpsRedirectUrlDelayed()
            throws TimeoutException {
        // Set the testing flag to simulate the case where the result has not yet arrived.
        // This should lead to returning activity result _after_ the verification flow resumes
        // to finish it in a delayed manner.
        AuthTabVerifier.setDelayVerificationForTesting(true);
        setUpAuthTab();
        var result =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var override = getOverrideUrlLoadingResult(AUTH_TAB_HTTPS_REDIRECT_URL);
                            AuthTabVerifier.setDelayVerificationForTesting(false);
                            mNavigationDelegate.resumeDelayedVerificationForTesting();
                            return override;
                        });
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_CLOSING_AFTER_AUTH, result.getResultType());
        Assert.assertTrue(mAuthTab.isFinishing());
    }

    /**
     * When loading a normal http url that chrome is able to handle, an intent picker should never
     * be shown, even if other activities such as {@link DummyActivityForHttp} claim to handle it.
     */
    @Test
    @SmallTest
    @DisableIf.Build(
            supported_abis_includes = "x86",
            sdk_is_greater_than = VERSION_CODES.O_MR1,
            sdk_is_less_than = VERSION_CODES.Q,
            message = "crbug.com/1188920")
    public void testIntentPickerNotShownForNormalUrl() throws TimeoutException {
        setUpTwa();
        final GURL testUrl = new GURL("http://customtabtest.com");
        RedirectHandler redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0, false, true);
        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(testUrl, false)
                        .setRedirectHandler(redirectHandler)
                        .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, result.getResultType());
    }

    private @VerificationStatus int getCurrentPageVerifierStatus() {
        CustomTabActivity customTabActivity = mCustomTabActivityTestRule.getActivity();
        return customTabActivity.getComponent().resolveCurrentPageVerifier().getState().status;
    }

    /**
     * Tests that {@link CustomTabNavigationDelegate#shouldDisableExternalIntentRequestsForUrl()}
     * disables forwarding URL requests to external intents for navigations within the TWA's origin.
     */
    @Test
    @SmallTest
    public void testShouldDisableExternalIntentRequestsForUrl() throws TimeoutException {
        setUpTwa();
        GURL insideVerifiedOriginUrl =
                new GURL(mTestServer.getURL("/chrome/test/data/android/simple.html"));
        GURL outsideVerifiedOriginUrl = new GURL("https://example.com/test.html");

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mCustomTabActivityTestRule.getActivity());
        Assert.assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());

        assertTrue(
                mNavigationDelegate.shouldDisableExternalIntentRequestsForUrl(
                        insideVerifiedOriginUrl));
        Assert.assertFalse(
                mNavigationDelegate.shouldDisableExternalIntentRequestsForUrl(
                        outsideVerifiedOriginUrl));
    }
}
