// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetCredentialOptions;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PrfValues;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.CreateConfirmationUiDelegate;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.GpmBrowserOptionsHelper;
import org.chromium.components.webauthn.InternalAuthenticator;
import org.chromium.components.webauthn.InternalAuthenticatorJni;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Arrays;

/** Unit tests for {@link AuthenticatorImpl} and {@link InternalAuthenticator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors",
})
@Batch(Batch.PER_CLASS)
@Restriction({
    GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_19W13,
    DeviceRestriction.RESTRICTION_TYPE_NON_AUTO
})
public class AuthenticatorImplTest {
    private static final String TAG = "AuthImplTest";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Mock UkmRecorder.Natives mUkmRecorderJniMock;

    private Context mContext;
    private WebauthnTestUtils.MockIntentSender mIntentSender;
    private EmbeddedTestServer mTestServer;
    private WebauthnTestUtils.MockAuthenticatorRenderFrameHost mFrameHost;
    private MockWebContents mWebContents;
    private WebauthnTestUtils.MockFido2ApiCallHelper mFido2ApiCallHelper;
    private Origin mOrigin;
    private WebauthnTestUtils.TestAuthenticatorImplJni mTestAuthenticatorImplJni;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private GetCredentialOptions mRequestOptions;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private long mStartTimeMs;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();
        mContext = ContextUtils.getApplicationContext();
        WebauthnTestUtils.applyFidoOverride(mContext);
        mIntentSender = new WebauthnTestUtils.MockIntentSender();
        mTestServer = mActivityTestRule.getTestServer();
        mCallback = Fido2ApiTestHelper.getAuthenticatorCallback();
        String url =
                mTestServer.getURLWithHostName(
                        "subdomain.example.test", "/content/test/data/android/authenticator.html");
        GURL gurl = new GURL(url);
        mOrigin = Origin.create(gurl);
        GpmBrowserOptionsHelper.createDefaultBrowserOptions();
        GpmBrowserOptionsHelper.setIsIncognitoExtraUntilTearDown(false);
        mActivityTestRule.loadUrl(url);
        mFrameHost = new WebauthnTestUtils.MockAuthenticatorRenderFrameHost();
        mFrameHost.setLastCommittedUrl(gurl);
        mWebContents = new MockWebContents();
        mWebContents.renderFrameHost = mFrameHost;

        mTestAuthenticatorImplJni = new WebauthnTestUtils.TestAuthenticatorImplJni(mCallback);
        InternalAuthenticatorJni.setInstanceForTesting(mTestAuthenticatorImplJni);
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mRequestOptions = new GetCredentialOptions();
        mRequestOptions.publicKey = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
        WebauthnModeProvider.getInstance().setGlobalWebauthnMode(WebauthnMode.CHROME);

        // Ensure AuthenticatorImpl doesn't hold onto a stale request override from another test.
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(null);

        mFido2ApiCallHelper = new WebauthnTestUtils.MockFido2ApiCallHelper();
        mFido2ApiCallHelper.setReturnedCredentialDetails(
                Arrays.asList(Fido2ApiTestHelper.getCredentialDetails()));
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        mStartTimeMs = SystemClock.elapsedRealtime();

        CredManSupportProvider.setupForTesting(
                /* overrideAndroidVersion= */ Build.VERSION_CODES.TIRAMISU,
                /* overrideForcesGpm= */ false);
    }

    @After
    public void tearDown() {
        InternalAuthenticatorJni.setInstanceForTesting(null);
        UkmRecorderJni.setInstanceForTesting(null);
        Fido2ApiCallHelper.overrideInstanceForTesting(null);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(null);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        /* createConfirmationUiDelegate= */ null,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.makeCredential(
                            mCreationOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onRegisterResponse(status, response));
                });

        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_withConfirmationUi_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        boolean[] wasCalled = new boolean[1];
        CreateConfirmationUiDelegate createConfirmationUiDelegate =
                (accept, reject) -> {
                    wasCalled[0] = true;
                    accept.run();
                    return true;
                };
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        createConfirmationUiDelegate,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.makeCredential(
                            mCreationOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onRegisterResponse(status, response));
                });

        mCallback.blockUntilCalled();
        Assert.assertTrue(wasCalled[0]);
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_withConfirmationUi_rejected() {
        boolean[] wasCalled = new boolean[1];
        CreateConfirmationUiDelegate createConfirmationUiDelegate =
                (accept, reject) -> {
                    wasCalled[0] = true;
                    reject.run();
                    return true;
                };
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        createConfirmationUiDelegate,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.makeCredential(
                            mCreationOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onRegisterResponse(status, response));
                });

        mCallback.blockUntilCalled();
        Assert.assertTrue(wasCalled[0]);
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_resultCanceled() {
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        /* createConfirmationUiDelegate= */ null,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.makeCredential(
                            mCreationOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onRegisterResponse(status, response));
                });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(
                        mContext, mIntentSender, mFrameHost, mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.makeCredential(mCreationOptions.serialize());
                });

        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        Fido2ApiTestHelper.validateMakeCredentialResponse(response);
        Assert.assertFalse(response.echoCredProps);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_resultCanceled() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(
                        mContext, mIntentSender, mFrameHost, mOrigin);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.makeCredential(mCreationOptions.serialize());
                });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertionWithUvmRequestedWithUvmResponded_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        /* createConfirmationUiDelegate= */ null,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.publicKey.extensions.userVerificationMethods = true;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getCredential(
                            mRequestOptions,
                            (getCredentialResponse) -> {
                                Assert.assertEquals(
                                        AuthenticatorStatus.SUCCESS,
                                        getCredentialResponse.getGetAssertionResponse().status);
                                mCallback.onSignResponse(
                                        getCredentialResponse.getGetAssertionResponse().credential,
                                        /* passwordCredential= */ null);
                            });
                });
        mCallback.blockUntilCalled();
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertionWithPrf_success() {
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        /* createConfirmationUiDelegate= */ null,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithPrf());
        PrfValues prfValues = new PrfValues();
        prfValues.first = new byte[] {1, 2, 3, 4, 5, 6};
        mRequestOptions.publicKey.extensions.prf = true;
        mRequestOptions.publicKey.extensions.prfInputs =
                new PrfValues[] {
                    prfValues,
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getCredential(
                            mRequestOptions,
                            (getCredentialResponse) -> {
                                Assert.assertEquals(
                                        AuthenticatorStatus.SUCCESS,
                                        getCredentialResponse.getGetAssertionResponse().status);
                                mCallback.onSignResponse(
                                        getCredentialResponse.getGetAssertionResponse().credential,
                                        /* passwordCredential= */ null);
                            });
                });
        mCallback.blockUntilCalled();

        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validatePrfResults(
                mCallback.getGetAssertionResponse().extensions.prfResults);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetCredential_resultCanceled() {
        AuthenticatorImpl authenticator =
                new AuthenticatorImpl(
                        mContext,
                        mWebContents,
                        mIntentSender,
                        /* createConfirmationUiDelegate= */ null,
                        mFrameHost,
                        mOrigin);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getCredential(
                            mRequestOptions,
                            (getCredentialResponse) -> {
                                Assert.assertEquals(
                                        AuthenticatorStatus.NOT_ALLOWED_ERROR,
                                        getCredentialResponse.getGetAssertionResponse().status);
                                mCallback.onSignResponse(
                                        getCredentialResponse.getGetAssertionResponse().credential,
                                        /* passwordCredential= */ null);
                            });
                });
        mCallback.blockUntilCalled();
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorGetCredentialWithUvmRequestedWithUvmResponded_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(
                        mContext, mIntentSender, mFrameHost, mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.publicKey.extensions.userVerificationMethods = true;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getAssertion(mRequestOptions.publicKey.serialize());
                });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorGetCredential_resultCanceled() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(
                        mContext, mIntentSender, mFrameHost, mOrigin);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getAssertion(mRequestOptions.publicKey.serialize());
                });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }
}
