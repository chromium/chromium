// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.webauthn.AuthenticatorErrorResponseCallback;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.IsUvpaaResponseCallback;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DeviceRestriction;

/** Test suite for navigator.credentials functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "enable-experimental-web-platform-features",
    "enable-features=WebAuthentication",
    "ignore-certificate-errors"
})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class AuthenticatorTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_FILE = "/content/test/data/android/authenticator.html";
    private EmbeddedTestServer mTestServer;
    private String mUrl;
    private WebPageStation mPage;
    private Tab mTab;
    private AuthenticatorUpdateWaiter mUpdateWaiter;
    @Mock private Fido2CredentialRequest mMockCredentialRequest;

    /** Waits until the JavaScript code supplies a result. */
    private class AuthenticatorUpdateWaiter extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;
        private String mStatus;

        public AuthenticatorUpdateWaiter() {
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
        mPage = mActivityTestRule.startOnBlankPage();
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        mUrl = mTestServer.getURLWithHostName("subdomain.example.test", TEST_FILE);
        mTab = mPage.getTab();
        mUpdateWaiter = new AuthenticatorUpdateWaiter();
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(mUpdateWaiter));
        WebauthnModeProvider.getInstance().setGlobalWebauthnMode(WebauthnMode.CHROME);
        doAnswer(
                        invocation -> {
                            AuthenticatorErrorResponseCallback errorCallback =
                                    invocation.getArgument(6);
                            errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
                            return null;
                        })
                .when(mMockCredentialRequest)
                .handleMakeCredentialRequest(
                        any(), any(), any(), any(), any(), any(), any(), any());
        doAnswer(
                        invocation -> {
                            AuthenticatorErrorResponseCallback errorCallback =
                                    invocation.getArgument(5);
                            errorCallback.onError(AuthenticatorStatus.NOT_IMPLEMENTED);
                            return null;
                        })
                .when(mMockCredentialRequest)
                .handleGetCredentialRequest(any(), any(), any(), any(), any(), any(), any());
        doAnswer(
                        invocation -> {
                            IsUvpaaResponseCallback callback = invocation.getArgument(0);
                            callback.onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
                            return null;
                        })
                .when(mMockCredentialRequest)
                .handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(any());
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(mMockCredentialRequest);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.removeObserver(mUpdateWaiter));
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for
     * navigator.credentials.create. This test currently expects a "Not Implemented" response.
     * Testing any real response would require setting up or mocking a real APK.
     */
    @Test
    @MediumTest
    @Feature({"WebAuth"})
    public void testCreatePublicKeyCredential() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("doCreatePublicKeyCredential()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for navigator.credentials.get.
     * This test currently expects a "Not Implemented" response. Testing any real response would
     * require setting up or mocking a real APK.
     */
    @Test
    @MediumTest
    @Feature({"WebAuth"})
    public void testGetPublicKeyCredential() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("doGetPublicKeyCredential()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for
     * PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable. This test currently
     * expects a "false" response.
     */
    @Test
    @MediumTest
    @Feature({"WebAuth"})
    public void testIsUserVerifyingPlatformAuthenticatorAvailable() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "doIsUserVerifyingPlatformAuthenticatorAvailable()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }

    /**
     * Verify that the Mojo bridge between Blink and Java is working for
     * PublicKeyCredential.isConditionalMediationAvailable.
     */
    @Test
    @MediumTest
    @Feature({"WebAuth"})
    public void testIsConditionalMediationAvailable() throws Exception {
        mActivityTestRule.loadUrl(mUrl);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("doIsConditionalMediationAvailable()");
        Assert.assertEquals("Success", mUpdateWaiter.waitForUpdate());
    }
}
