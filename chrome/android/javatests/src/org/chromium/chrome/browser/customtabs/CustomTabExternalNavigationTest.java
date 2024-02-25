// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Intent;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
    private CustomTabNavigationDelegate mNavigationDelegate;
    private EmbeddedTestServer mTestServer;
    private ExternalNavigationHandler mUrlHandler;

    @Before
    public void setUp() throws Exception {
        mCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = mCustomTabActivityTestRule.getTestServer();

        launchTwa(TWA_PACKAGE_NAME, mTestServer.getURL(TEST_PATH));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(tab);
        Assert.assertTrue(delegateFactory instanceof CustomTabDelegateFactory);
        CustomTabDelegateFactory customTabDelegateFactory =
                ((CustomTabDelegateFactory) delegateFactory);
        mUrlHandler =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> customTabDelegateFactory.createExternalNavigationHandler(tab));
        Assert.assertTrue(
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

    /**
     * For urls with special schemes and hosts, and there is exactly one activity having a matching
     * intent filter, the framework will make that activity the default handler of the special url.
     * This test tests whether chrome is able to start the default external handler.
     */
    @Test
    @SmallTest
    public void testExternalActivityStartedForDefaultUrl() {
        ExternalNavigationHandler.sAllowIntentsToSelfForTesting = true;
        final GURL testUrl = new GURL("customtab://customtabtest/intent");
        RedirectHandler redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0, false, true);
        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(testUrl, false)
                        .setIsMainFrame(true)
                        .setIsRendererInitiated(true)
                        .setRedirectHandler(redirectHandler)
                        .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, result.getResultType());
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
    public void testIntentPickerNotShownForNormalUrl() {
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
        GURL insideVerifiedOriginUrl =
                new GURL(mTestServer.getURL("/chrome/test/data/android/simple.html"));
        GURL outsideVerifiedOriginUrl = new GURL("https://example.com/test.html");

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mCustomTabActivityTestRule.getActivity());
        Assert.assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());

        Assert.assertTrue(
                mNavigationDelegate.shouldDisableExternalIntentRequestsForUrl(
                        insideVerifiedOriginUrl));
        Assert.assertFalse(
                mNavigationDelegate.shouldDisableExternalIntentRequestsForUrl(
                        outsideVerifiedOriginUrl));
    }
}
