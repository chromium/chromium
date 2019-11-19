// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/** Test suite for navigator.credentials functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-experimental-web-platform-features", "enable-features=WebAuthentication",
        "ignore-certificate-errors"})
public class AuthenticatorTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_FILE = "/content/test/data/android/authenticator.html";
    private EmbeddedTestServer mTestServer;
    private String mUrl;
    private Tab mTab;
    private AuthenticatorUpdateWaiter mUpdateWaiter;
    private MockFido2ApiHandler mMockHandler;

    private class MockFido2ApiHandler extends Fido2ApiHandler {
        @Override
        protected void makeCredential(PublicKeyCredentialCreationOptions options,
                RenderFrameHost frameHost, HandlerResponseCallback callback) {
            callback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
        }

        @Override
        protected void getAssertion(PublicKeyCredentialRequestOptions options,
                RenderFrameHost frameHost, HandlerResponseCallback callback) {
            callback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
        }

        @Override
        protected void isUserVerifyingPlatformAuthenticatorAvailable(
                RenderFrameHost frameHost, HandlerResponseCallback callback) {
            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
        }
    }

    /** Waits until the JavaScript code supplies a result. */
    private class AuthenticatorUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mStatus;

        public AuthenticatorUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String title = mActivityTestRule.getActivity().getActivityTab().getTitle();

            // Wait until the title indicates either success or failure.
            if (!title.startsWith("Success") && !title.startsWith("Fail:")) return;
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
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        mUrl = mTestServer.getURLWithHostName("subdomain.example.test", TEST_FILE);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mUpdateWaiter = new AuthenticatorUpdateWaiter();
        mTab.addObserver(mUpdateWaiter);
        mMockHandler = new MockFido2ApiHandler();
    }

    @After
    public void tearDown() {
        mTab.removeObserver(mUpdateWaiter);
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for
     * navigator.credentials.create. This test currently expects a
     * "Not Implemented" response. Testing any real response would require
     * setting up or mocking a real APK.
     */
    @Test
    @DisableIf.Build(sdk_is_less_than = 24)
    @MediumTest
    @Feature({"WebAuth"})
    public void testCreatePublicKeyCredential() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        Fido2ApiHandler.overrideInstanceForTesting(mMockHandler);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("doCreatePublicKeyCredential()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for
     * navigator.credentials.get. This test currently expects a
     * "Not Implemented" response. Testing any real response would require
     * setting up or mocking a real APK.
     */
    @Test
    @DisableIf.Build(sdk_is_less_than = 24)
    @MediumTest
    @Feature({"WebAuth"})
    public void testGetPublicKeyCredential() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        Fido2ApiHandler.overrideInstanceForTesting(mMockHandler);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("doGetPublicKeyCredential()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for
     * PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable.
     * This test currently expects a "false" response.
     */
    @Test
    @DisableIf.Build(sdk_is_less_than = 24)
    @MediumTest
    @Feature({"WebAuth"})
    public void testIsUserVerifyingPlatformAuthenticatorAvailable() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        Fido2ApiHandler.overrideInstanceForTesting(mMockHandler);
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "doIsUserVerifyingPlatformAuthenticatorAvailable()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }
}
