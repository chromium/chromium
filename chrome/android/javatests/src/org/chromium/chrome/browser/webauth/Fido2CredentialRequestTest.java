// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;


import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;
import android.util.Base64;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.GetCredentialOptions;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.Mediation;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.RemoteDesktopClientOverride;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.Fido2Api;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.FidoIntentSender;
import org.chromium.components.webauthn.GetAssertionOutcome;
import org.chromium.components.webauthn.GmsCoreUtils;
import org.chromium.components.webauthn.GpmBrowserOptionsHelper;
import org.chromium.components.webauthn.InternalAuthenticatorJni;
import org.chromium.components.webauthn.MakeCredentialOutcome;
import org.chromium.components.webauthn.WebauthnBrowserBridge;
import org.chromium.components.webauthn.WebauthnCredentialDetails;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content.browser.ClientDataJsonImplJni;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.mojo_base.mojom.String16;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.util.RunnableTimer;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link Fido2CredentialRequest}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors",
})
@Batch(Batch.PER_CLASS)
@Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_19W13)
public class Fido2CredentialRequestTest {
    private static final String TAG = "Fido2CredentialRequestTest";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Mock ClientDataJsonImpl.Natives mClientDataJsonImplMock;
    @Mock UkmRecorder.Natives mUkmRecorderJniMock;

    private WebPageStation mPage;
    private Context mContext;
    private WebauthnTestUtils.MockIntentSender mIntentSender;
    private EmbeddedTestServer mTestServer;
    private WebauthnTestUtils.MockAuthenticatorRenderFrameHost mFrameHost;
    private MockWebContents mWebContents;
    private WebauthnTestUtils.MockBrowserBridge mMockBrowserBridge;
    private WebauthnTestUtils.MockFido2ApiCallHelper mFido2ApiCallHelper;
    private AuthenticationContextProvider mAuthenticationContextProvider;
    private Origin mOrigin;
    private Bundle mBrowserOptions;
    private WebauthnTestUtils.TestAuthenticatorImplJni mTestAuthenticatorImplJni;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private GetCredentialOptions mRequestOptions;
    private static final String FILLER_ERROR_MSG = "Error Error";
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private long mStartTimeMs;
    private static final String PASSWORD_CRED_USERNAME = "Monkey";
    private static String16 sPasswordCredUsername16;
    private static final String PASSWORD_CRED_PASSWORD = "DLuffy2";
    private static String16 sPasswordCredPassword16;

    /**
     * This class constructs the parameters array that is used for testMakeCredential_with_param and
     * testGetAssertion_with_param as input parameters.
     */
    public static class ErrorTestParams implements ParameterProvider {
        private static final List<ParameterSet> sErrorTestParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(
                                        Fido2Api.SECURITY_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.INVALID_DOMAIN))
                                .name("securityError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.TIMEOUT_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR))
                                .name("timeoutError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.ENCODING_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                                .name("encodingError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.NOT_ALLOWED_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR))
                                .name("notAllowedError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.DATA_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(
                                                AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR))
                                .name("dataError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.NOT_SUPPORTED_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(
                                                AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR))
                                .name("notSupportedError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.CONSTRAINT_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                                .name("constraintErrorReRegistration"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.INVALID_STATE_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                                .name("invalidStateError"),
                        new ParameterSet()
                                .value(
                                        Fido2Api.UNKNOWN_ERR,
                                        FILLER_ERROR_MSG,
                                        Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                                .name("unknownError"));

        @Override
        public List<ParameterSet> getParameters() {
            return sErrorTestParams;
        }
    }

    public static class SameOriginTestParams implements ParameterProvider {
        private static final List<ParameterSet> sSameOriginTestParams =
                Arrays.asList(
                        new ParameterSet().value(true).name("SameOrigin"),
                        new ParameterSet().value(false).name("CrossOrigin"));

        @Override
        public List<ParameterSet> getParameters() {
            return sSameOriginTestParams;
        }
    }

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
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
        mBrowserOptions = GpmBrowserOptionsHelper.createDefaultBrowserOptions();
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
        mAuthenticationContextProvider =
                new AuthenticationContextProvider() {
                    @Override
                    public Context getContext() {
                        return mContext;
                    }

                    @Override
                    public RenderFrameHost getRenderFrameHost() {
                        return mFrameHost;
                    }

                    @Override
                    public FidoIntentSender getIntentSender() {
                        return mIntentSender;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        mRequest = new Fido2CredentialRequest(mAuthenticationContextProvider);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(mRequest);

        mFido2ApiCallHelper = new WebauthnTestUtils.MockFido2ApiCallHelper();
        mFido2ApiCallHelper.setReturnedCredentialDetails(
                Arrays.asList(Fido2ApiTestHelper.getCredentialDetails()));
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        sPasswordCredUsername16 =
                WebauthnBrowserBridge.stringToMojoString16(PASSWORD_CRED_USERNAME);
        sPasswordCredPassword16 =
                WebauthnBrowserBridge.stringToMojoString16(PASSWORD_CRED_PASSWORD);

        mMockBrowserBridge =
                new WebauthnTestUtils.MockBrowserBridge(
                        sPasswordCredUsername16, sPasswordCredPassword16);
        mRequest.overrideBrowserBridgeForTesting(mMockBrowserBridge);

        mStartTimeMs = SystemClock.elapsedRealtime();

        CredManSupportProvider.setupForTesting(
                /* overrideAndroidVersion= */ Build.VERSION_CODES.TIRAMISU,
                /* overrideForcesGpm= */ false);
    }

    @Test
    @SmallTest
    public void testConvertOriginToString_defaultPortRemoved() {
        Origin origin = Origin.create(new GURL("https://www.example.com:443"));
        String parsedOrigin = Fido2CredentialRequest.convertOriginToString(origin);
        Assert.assertEquals("https://www.example.com/", parsedOrigin);
    }

    @Test
    @SmallTest
    public void testMakeCredential_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        Fido2ApiTestHelper.mockClientDataJson();

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        // Only error outcomes are reported through the outcome callback.
        Assert.assertEquals(mCallback.getOutcome(), null);
    }

    @Test
    @SmallTest
    public void testPasskeyMakeCredential_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulPasskeyMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));

        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        Assert.assertArrayEquals(
                response.transports,
                new int[] {
                    AuthenticatorTransport.BLE,
                    AuthenticatorTransport.HYBRID,
                    AuthenticatorTransport.INTERNAL,
                    AuthenticatorTransport.NFC,
                    AuthenticatorTransport.USB
                });
        Assert.assertEquals(AuthenticatorAttachment.PLATFORM, response.authenticatorAttachment);
    }

    @Test
    @SmallTest
    public void testMakeCredential_unsuccessfulAttemptToShowCancelableIntent() {
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_missingExtra() {
        // An intent missing FIDO2_KEY_RESPONSE_EXTRA.
        mIntentSender.setNextResultIntent(new Intent());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_nullIntent() {
        // Don't set an intent to be returned at all.
        mIntentSender.setNextResultIntent(null);

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultCanceled() {
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(), Integer.valueOf(MakeCredentialOutcome.USER_CANCELLATION));
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultUnknown() {
        mIntentSender.setNextResult(
                Activity.RESULT_FIRST_USER,
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_noEligibleParameters() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = 1; // Not a valid algorithm identifier.
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        customOptions.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ALGORITHM_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(),
                Integer.valueOf(MakeCredentialOutcome.ALGORITHM_NOT_SUPPORTED));
    }

    @Test
    @SmallTest
    public void testMakeCredential_parametersContainEligibleAndNoneligible() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = 1; // Not a valid algorithm identifier.
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        PublicKeyCredentialParameters[] multiParams =
                new PublicKeyCredentialParameters[] {
                    customOptions.publicKeyParameters[0], parameters
                };
        customOptions.publicKeyParameters = multiParams;
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        Fido2ApiTestHelper.mockClientDataJson();

        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_noExcludeCredentials() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        Fido2ApiTestHelper.mockClientDataJson();

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.excludeCredentials = new PublicKeyCredentialDescriptor[0];
        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_attestationIncluded() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithAttestation());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        WebauthnTestUtils.assertHasAttestation(response);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_rkRequired_attestationKept()
            throws Exception {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithAttestation());

        // Set the residentKey option to trigger attestation removal.
        PublicKeyCredentialCreationOptions creationOptions =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        creationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.REQUIRED;

        mRequest.handleMakeCredentialRequest(
                creationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        WebauthnTestUtils.assertHasAttestation(response);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_rkPreferred_attestationKept()
            throws Exception {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithAttestation());

        // Set the residentKey option to trigger attestation removal.
        PublicKeyCredentialCreationOptions creationOptions =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        creationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.PREFERRED;

        mRequest.handleMakeCredentialRequest(
                creationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        WebauthnTestUtils.assertHasAttestation(response);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_credprops() throws Exception {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithCredProps());

        PublicKeyCredentialCreationOptions creationOptions =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        creationOptions.credProps = true;

        mRequest.handleMakeCredentialRequest(
                creationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        Assert.assertTrue(response.echoCredProps);
        Assert.assertTrue(response.hasCredPropsRk);
        Assert.assertTrue(response.credPropsRk);
    }

    @Test
    @SmallTest
    public void testGetCredentialWithoutUvmRequested_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        Fido2ApiTestHelper.mockClientDataJson();

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetCredentialWithUvmRequestedWithoutUvmResponded_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequestOptions.publicKey.extensions.userVerificationMethods = true;
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetCredentialWithUvmRequestedWithUvmResponded_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        mRequestOptions.publicKey.extensions.userVerificationMethods = true;
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Assert.assertTrue(response.extensions.echoUserVerificationMethods);
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetCredentialWithUserId_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUserId());

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Assert.assertArrayEquals(response.userHandle, Fido2ApiTestHelper.TEST_USER_HANDLE);
    }

    @Test
    @SmallTest
    public void testGetCredential_unsuccessfulAttemptToShowCancelableIntent() {
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetCredential_missingExtra() {
        // An intent missing FIDO2_KEY_RESPONSE_EXTRA.
        mIntentSender.setNextResultIntent(new Intent());

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetCredential_nullIntent() {
        // Don't set an intent to be returned at all.
        mIntentSender.setNextResultIntent(null);

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetCredential_resultCanceled() {
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(), Integer.valueOf(GetAssertionOutcome.USER_CANCELLATION));
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultUnknown() {
        mIntentSender.setNextResult(
                Activity.RESULT_FIRST_USER,
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_unknownErrorCredentialNotRecognized() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.UNKNOWN_ERR, "Low level error 0x6a80"));

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(),
                Integer.valueOf(GetAssertionOutcome.CREDENTIAL_NOT_RECOGNIZED));
    }

    @Test
    @SmallTest
    public void testGetAssertion_appIdUsed() {
        Fido2ApiTestHelper.mockClientDataJson();
        GetCredentialOptions customOptions = mRequestOptions;
        customOptions.publicKey.extensions.appid = "www.example.com";
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Assert.assertEquals(true, response.extensions.echoAppidExtension);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationNone() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.NONE;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationIndirect() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.INDIRECT;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationDirect() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.DIRECT;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationEnterprise() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.ENTERPRISE;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_invalidStateErrorDuplicateRegistration() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(), Integer.valueOf(MakeCredentialOutcome.CREDENTIAL_EXCLUDED));
    }

    @Test
    @SmallTest
    @DisableFeatures(BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_BROWSER_BOUND_KEYS)
    public void testMakeCredential_isPaymentCredentialCreationPassedToFrameHost() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));

        mCreationOptions.isPaymentCredentialCreation = true;
        Assert.assertFalse(mFrameHost.isPaymentCredentialCreation());
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        Assert.assertTrue(mFrameHost.isPaymentCredentialCreation());
    }

    @Test
    @SmallTest
    @EnableFeatures(BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_BROWSER_BOUND_KEYS)
    public void
            testMakeCredential_isPaymentCredentialCreationPassedToFrameHostWithPaymentOptions() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));

        mCreationOptions.isPaymentCredentialCreation = true;
        Assert.assertFalse(mFrameHost.isPaymentCredentialCreation());
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                Fido2ApiTestHelper.createPaymentOptions(),
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        Assert.assertTrue(mFrameHost.isPaymentCredentialCreation());
    }

    @Test
    @SmallTest
    public void testGetCredential_emptyAllowCredentials1() {
        // Passes conversion and gets rejected by GmsCore
        GetCredentialOptions customOptions = mRequestOptions;
        customOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.NOT_ALLOWED_ERR,
                        "Authentication request must have non-empty allowList"));

        // Requests with empty allowCredentials are only passed to GMSCore if there are no
        // local passkeys available.
        mFido2ApiCallHelper.setReturnedCredentialDetails(new ArrayList<>());

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(), Integer.valueOf(GetAssertionOutcome.RK_NOT_SUPPORTED));
    }

    @Test
    @SmallTest
    public void testGetCredential_emptyAllowCredentials2() {
        // Passes conversion and gets rejected by GmsCore
        GetCredentialOptions customOptions = mRequestOptions;
        customOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.NOT_ALLOWED_ERR,
                        "Request doesn't have a valid list of allowed credentials."));

        // Requests with empty allowCredentials are only passed to GMSCore if there are no
        // local passkeys available.
        mFido2ApiCallHelper.setReturnedCredentialDetails(new ArrayList<>());

        mRequest.handleGetCredentialRequest(
                customOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_constraintErrorNoScreenlock() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.CONSTRAINT_ERR, "The device is not secured with any screen lock"));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(), Integer.valueOf(MakeCredentialOutcome.UV_NOT_SUPPORTED));
    }

    @Test
    @SmallTest
    public void testGetAssertion_constraintErrorNoScreenlock() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.CONSTRAINT_ERR, "The device is not secured with any screen lock"));

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(
                mCallback.getOutcome(), Integer.valueOf(GetAssertionOutcome.UV_NOT_SUPPORTED));
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testMakeCredential_with_param(Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(errorCode, errorMsg));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testGetCredential_with_param(Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(errorCode, errorMsg));

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testMakeCredential_with_param_nullErrorMessage(
            Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(errorCode, null));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testGetCredential_with_param_nullErrorMessage(
            Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(errorCode, null));

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_securePaymentConfirmation_canReplaceClientDataJson() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        String clientDataJson = "520";
        Fido2ApiTestHelper.mockClientDataJson(clientDataJson);

        mFrameHost.setLastCommittedUrl(new GURL("https://www.chromium.org/pay"));

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.publicKey.challenge = new byte[3];
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                payment,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertEquals(
                new String(
                        mCallback.getGetAssertionResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_securePaymentConfirmation_clientDataJsonCannotBeEmpty() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        Fido2ApiTestHelper.mockClientDataJson(null);

        mFrameHost.setLastCommittedUrl(new GURL("https://www.chromium.org/pay"));

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.publicKey.challenge = new byte[3];
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                payment,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_securePaymentConfirmation_buildClientDataJsonParameters() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        Origin topOrigin = Origin.create(new GURL("https://www.chromium.org/pay"));

        // ClientDataJsonImplJni is mocked directly instead of using
        // Fido2ApiTestHelper.mockClientDataJson so that it can be used to verify the call arguments
        // below.
        ClientDataJsonImplJni.setInstanceForTesting(mClientDataJsonImplMock);

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.publicKey.challenge = new byte[3];
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                topOrigin,
                payment,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);

        ArgumentCaptor<Origin> topOriginCaptor = ArgumentCaptor.forClass(Origin.class);
        Mockito.verify(mClientDataJsonImplMock, Mockito.times(1))
                .buildClientDataJson(
                        eq(ClientDataRequestType.PAYMENT_GET),
                        eq(Fido2CredentialRequest.convertOriginToString(mOrigin)),
                        eq(mRequestOptions.publicKey.challenge),
                        eq(false),
                        eq(payment.serialize()),
                        eq(mRequestOptions.publicKey.relyingPartyId),
                        topOriginCaptor.capture());

        String topOriginString =
                Fido2CredentialRequest.convertOriginToString(topOriginCaptor.getValue());
        String expectedTopOriginString = Fido2CredentialRequest.convertOriginToString(topOrigin);
        Assert.assertEquals(expectedTopOriginString, topOriginString);
    }

    @Test
    @SmallTest
    public void testGetAssertion_emptyAllowCredentials_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        Fido2ApiTestHelper.mockClientDataJson();

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_emptyAllowCredentialsUserCancels_notAllowedError() {
        mMockBrowserBridge.setInvokeCallbackImmediately(
                WebauthnTestUtils.MockBrowserBridge.CallbackInvocationType.USER_DISMISSED_UI);
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUi_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        Fido2ApiTestHelper.mockClientDataJson();

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
        Assert.assertEquals(mCallback.getOutcome(), null);
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUi_errorFromNative() {
        mMockBrowserBridge.setInvokeCallbackImmediately(
                WebauthnTestUtils.MockBrowserBridge.CallbackInvocationType.ERROR);
        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiNondiscoverableCredential_failure() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        WebauthnCredentialDetails nonDiscoverableCredDetails =
                Fido2ApiTestHelper.getCredentialDetails();
        nonDiscoverableCredDetails.mIsDiscoverable = false;
        mFido2ApiCallHelper.setReturnedCredentialDetails(Arrays.asList(nonDiscoverableCredDetails));
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUi_cancelWhileFetchingCredentials() {
        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mFido2ApiCallHelper.setInvokeCallbackImmediately(false);

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mRequest.cancelGetAssertion();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.ABORT_ERROR), mCallback.getStatus());

        // Also validate that when the FIDO getCredentials call is completed, nothing happens.
        // The MockBrowserBridge will assert if onCredentialsDetailsListReceived is called.
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());
        mFido2ApiCallHelper.invokeSuccessCallback();
        Assert.assertEquals(0, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUi_cancelWhileWaitingForSelection() {
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        mMockBrowserBridge.setInvokeCallbackImmediately(
                WebauthnTestUtils.MockBrowserBridge.CallbackInvocationType.DELAYED);
        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mRequest.cancelGetAssertion();

        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.ABORT_ERROR), mCallback.getStatus());
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiCancelWhileRequestSentToPlatform_ignored() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        Fido2ApiTestHelper.mockClientDataJson();

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mIntentSender.setInvokeCallbackImmediately(false);

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mIntentSender.blockUntilShowIntentCalled();
        mRequest.cancelGetAssertion();
        mIntentSender.invokeCallback();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiCancelWhileRequestSentToPlatformUserDeny_cancelled() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR, ""));
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mIntentSender.setInvokeCallbackImmediately(false);

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mIntentSender.blockUntilShowIntentCalled();
        mRequest.cancelGetAssertion();
        mIntentSender.invokeCallback();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.ABORT_ERROR), mCallback.getStatus());
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiRequestSentToPlatformUserDeny_doesNotComplete() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR, ""));
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mIntentSender.blockUntilShowIntentCalled();
        // Null status indicates the callback was not invoked.
        Assert.assertNull(mCallback.getStatus());
        Assert.assertEquals(0, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiRetryAfterUserDeny_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR, ""));
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        Fido2ApiTestHelper.mockClientDataJson();

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mIntentSender.blockUntilShowIntentCalled();
        // Null status indicates the callback was not invoked.
        Assert.assertNull(mCallback.getStatus());
        Assert.assertEquals(0, mMockBrowserBridge.getCleanupCalledCount());

        // Select credential again, and provide a success response this time.
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.invokePasskeyCallback();
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiWithAllowCredentialMatch_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        Fido2ApiTestHelper.mockClientDataJson();

        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiWithAllowCredentialMismatch_failure() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {3, 2, 1};
        descriptor.transports = new int[] {0};
        mRequestOptions.publicKey.allowCredentials =
                new PublicKeyCredentialDescriptor[] {descriptor};
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    @Test
    @SmallTest
    public void testGetAssertion_immediateWithPasskeysOnly_success() {
        Fido2ApiTestHelper.mockClientDataJson();
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.IMMEDIATE;
        mRequestOptions.password = true;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_immediateWithNonEmptyAllowList_notAllowedError() {
        WebauthnBrowserBridge mockedBrowserBridge = Mockito.mock(WebauthnBrowserBridge.class);
        mRequest.overrideBrowserBridgeForTesting(mockedBrowserBridge);
        mRequestOptions.mediation = Mediation.IMMEDIATE;
        mRequestOptions.password = true;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Mockito.verify(mockedBrowserBridge, never())
                .onCredentialsDetailsListReceived(any(), any(), anyInt(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_immediateWithNoPasskeysFound_notAllowedError() {
        WebauthnBrowserBridge mockedBrowserBridge = Mockito.mock(WebauthnBrowserBridge.class);
        mRequest.overrideBrowserBridgeForTesting(mockedBrowserBridge);
        mFido2ApiCallHelper.setReturnedCredentialDetails(new ArrayList<>());
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.IMMEDIATE;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Mockito.verify(mockedBrowserBridge, never())
                .onCredentialsDetailsListReceived(any(), any(), anyInt(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_immediateWithNoPasskeysButPasswordReturned_success() {
        mMockBrowserBridge.setInvokeCallbackImmediately(
                WebauthnTestUtils.MockBrowserBridge.CallbackInvocationType.IMMEDIATE_PASSWORD);
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        mFido2ApiCallHelper.setReturnedCredentialDetails(new ArrayList<>());

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.IMMEDIATE;
        mRequestOptions.password = true;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Assert.assertEquals(
                sPasswordCredUsername16, mCallback.getGetAssertionPasswordCredential().name);
        Assert.assertEquals(
                sPasswordCredPassword16, mCallback.getGetAssertionPasswordCredential().password);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_immediateInIncognito_notAllowedError() {
        mWebContents = new WebauthnTestUtils.MockIncognitoWebContents();
        WebauthnBrowserBridge mockedBrowserBridge = Mockito.mock(WebauthnBrowserBridge.class);
        mRequest.overrideBrowserBridgeForTesting(mockedBrowserBridge);

        mRequestOptions.mediation = Mediation.IMMEDIATE;
        mRequestOptions.password = true;
        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Mockito.verify(mockedBrowserBridge, never())
                .onCredentialsDetailsListReceived(any(), any(), anyInt(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_immediateTimesOut_notAllowedError() {
        RunnableTimer timer = Mockito.mock(RunnableTimer.class);
        mRequest.setImmediateTimerForTesting(timer);

        doAnswer(
                        answer -> {
                            Runnable runnable = answer.getArgument(1);
                            runnable.run();
                            return null;
                        })
                .when(timer)
                .startTimer(anyLong(), any(Runnable.class));

        // Prevent credentials from being returned.
        mFido2ApiCallHelper.setInvokeCallbackImmediately(false);

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.IMMEDIATE;
        mRequestOptions.password = true;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
    }

    @Test
    @SmallTest
    public void testGetMatchingCredentialIds_success() {
        String relyingPartyId = "subdomain.example.test";
        byte[][] allowCredentialIds =
                new byte[][] {
                    {1, 2, 3},
                    {10, 11, 12},
                    {13, 14, 15},
                };
        boolean requireThirdPartyPayment = false;

        WebauthnCredentialDetails credential1 = Fido2ApiTestHelper.getCredentialDetails();
        credential1.mCredentialId = new byte[] {1, 2, 3};
        credential1.mIsPayment = true;

        WebauthnCredentialDetails credential2 = Fido2ApiTestHelper.getCredentialDetails();
        credential2.mCredentialId = new byte[] {4, 5, 6};
        credential2.mIsPayment = false;

        WebauthnCredentialDetails credential3 = Fido2ApiTestHelper.getCredentialDetails();
        credential3.mCredentialId = new byte[] {7, 8, 9};
        credential3.mIsPayment = true;

        WebauthnCredentialDetails credential4 = Fido2ApiTestHelper.getCredentialDetails();
        credential4.mCredentialId = new byte[] {10, 11, 12};
        credential4.mIsPayment = false;

        mFido2ApiCallHelper.setReturnedCredentialDetails(
                Arrays.asList(credential1, credential2, credential3, credential4));

        mRequest.handleGetMatchingCredentialIdsRequest(
                relyingPartyId,
                allowCredentialIds,
                requireThirdPartyPayment,
                mCallback::onGetMatchingCredentialIds,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertArrayEquals(
                mCallback.getGetMatchingCredentialIdsResponse().toArray(),
                new byte[][] {{1, 2, 3}, {10, 11, 12}});
    }

    @Test
    @SmallTest
    public void testGetMatchingCredentialIds_requireThirdPartyBit() {
        String relyingPartyId = "subdomain.example.test";
        byte[][] allowCredentialIds =
                new byte[][] {
                    {1, 2, 3},
                    {10, 11, 12},
                    {13, 14, 15},
                };
        boolean requireThirdPartyPayment = true;

        WebauthnCredentialDetails credential1 = Fido2ApiTestHelper.getCredentialDetails();
        credential1.mCredentialId = new byte[] {1, 2, 3};
        credential1.mIsPayment = true;

        WebauthnCredentialDetails credential2 = Fido2ApiTestHelper.getCredentialDetails();
        credential2.mCredentialId = new byte[] {4, 5, 6};
        credential2.mIsPayment = false;

        WebauthnCredentialDetails credential3 = Fido2ApiTestHelper.getCredentialDetails();
        credential3.mCredentialId = new byte[] {7, 8, 9};
        credential3.mIsPayment = true;

        WebauthnCredentialDetails credential4 = Fido2ApiTestHelper.getCredentialDetails();
        credential4.mCredentialId = new byte[] {10, 11, 12};
        credential4.mIsPayment = false;

        mFido2ApiCallHelper.setReturnedCredentialDetails(
                Arrays.asList(credential1, credential2, credential3, credential4));

        mRequest.handleGetMatchingCredentialIdsRequest(
                relyingPartyId,
                allowCredentialIds,
                requireThirdPartyPayment,
                mCallback::onGetMatchingCredentialIds,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertArrayEquals(
                mCallback.getGetMatchingCredentialIdsResponse().toArray(),
                new byte[][] {{1, 2, 3}});
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiHybrid_success() {
        if (!GmsCoreUtils.isHybridClientApiSupported()) {
            return;
        }

        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(Fido2ApiTestHelper.getCredentialDetails()));
        mMockBrowserBridge.setInvokeCallbackImmediately(
                WebauthnTestUtils.MockBrowserBridge.CallbackInvocationType.IMMEDIATE_HYBRID);
        Fido2ApiTestHelper.mockClientDataJson();

        mRequestOptions.publicKey.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.mediation = Mediation.CONDITIONAL;

        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(1, mMockBrowserBridge.getCleanupCalledCount());
    }

    private static class TestParcelable implements Parcelable {
        static final String CLASS_NAME =
                "org.chromium.chrome.browser.webauth.Fido2CredentialRequestTest$TestParcelable";

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(42);
        }
    }

    @Test
    @SmallTest
    public void parcelWriteValue_knownFormat() {
        // This test confirms that the format of Parcel.writeValue is as
        // expected. Sadly, Fido2Api needs to be sensitive to this because,
        // in one place, the interactions with the FIDO module are not just
        // passing SafeParcelable objects around, but rather an array of
        // them. This pulls in higher-level aspect of the Parcel class that
        // can change from release to release. We can't just use the Parcel
        // class because it's tightly coupled with assumptions about using
        // ClassLoaders for deserialisation.

        byte[][] encodings = new byte[2][];
        for (int i = 0; i < encodings.length; i++) {
            Parcel parcel = Parcel.obtain();
            parcel.writeInt(16 /* VAL_PARCELABLEARRRAY */);
            if (i > 0) {
                // include length prefix
                final int l = TestParcelable.CLASS_NAME.length();
                parcel.writeInt(
                        4 /* array length */
                                + 4 /* string length */
                                + 2 * (l + (l % 2)) /* two bytes per char, 4-byte aligned */
                                + 4 /* parcelable contents */);
            }
            parcel.writeInt(1); // array length
            parcel.writeString(TestParcelable.CLASS_NAME);
            parcel.writeInt(42); // Parcelable contents.

            encodings[i] = parcel.marshall();
            parcel.recycle();
        }

        Parcel parcel = Parcel.obtain();
        parcel.writeValue(new Parcelable[] {new TestParcelable()});
        byte[] data = parcel.marshall();

        boolean ok = false;
        for (final byte[] encoding : encodings) {
            if (Arrays.equals(encoding, data)) {
                ok = true;
                break;
            }
        }

        parcel.recycle();
        if (ok) {
            return;
        }

        Log.e(TAG, "Encoding was: " + Base64.encodeToString(data, Base64.NO_WRAP));
        for (final byte[] encoding : encodings) {
            Log.e(TAG, "    expected: " + Base64.encodeToString(encoding, Base64.NO_WRAP));
        }

        // If you're here because this test has broken. Firstly, I'm sorry.
        // Find agl@ and demand he fix it. The logcat output from the failing
        // test will be very helpful. If I'm unavailable then look in Android's
        // Parcel.java to figure out what changed in writeValue(), update
        // Fido2Api.java around the use of VAL_PARCELABLE and update this test
        // to have a 3rd acceptable encoding that matches the changes.
        Assert.fail("No matching encoding found");
    }

    @Test
    @SmallTest
    public void testGetAssertion_generatesClientDataJson() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        String clientDataJson = "520";
        Fido2ApiTestHelper.mockClientDataJson(clientDataJson);

        mFrameHost.setLastCommittedUrl(new GURL("https://www.example.com"));

        mRequestOptions.publicKey.challenge = new byte[3];
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertEquals(
                new String(
                        mCallback.getGetAssertionResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_generatesClientDataJson() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        String clientDataJson = "520";
        Fido2ApiTestHelper.mockClientDataJson(clientDataJson);

        mFrameHost.setLastCommittedUrl(new GURL("https://www.example.com"));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertEquals(
                new String(
                        mCallback.getMakeCredentialResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @EnableFeatures(BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_BROWSER_BOUND_KEYS)
    public void testMakeCredential_setsPaymentOptionsWhenPaymentCredential() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        String clientDataJson = "fakeClientDataJson";
        // Mock ClientDataJsonImplJni to verify the call arguments below.
        ClientDataJsonImplJni.setInstanceForTesting(mClientDataJsonImplMock);
        when(mClientDataJsonImplMock.buildClientDataJson(
                        anyInt(), any(), any(), anyBoolean(), any(), any(), any()))
                .thenReturn(clientDataJson);
        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mCreationOptions.isPaymentCredentialCreation = true;

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                payment,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();

        Mockito.verify(mClientDataJsonImplMock, Mockito.times(1))
                .buildClientDataJson(
                        eq(ClientDataRequestType.WEB_AUTHN_CREATE),
                        eq(Fido2CredentialRequest.convertOriginToString(mOrigin)),
                        eq(mCreationOptions.challenge),
                        /* isCrossOrigin= */ eq(false),
                        eq(payment.serialize()),
                        eq(mCreationOptions.relyingParty.name),
                        eq(mOrigin));
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Assert.assertEquals(
                new String(
                        mCallback.getMakeCredentialResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
    }

    @Test
    @SmallTest
    @EnableFeatures(BlinkFeatures.SECURE_PAYMENT_CONFIRMATION_BROWSER_BOUND_KEYS)
    public void testMakeCredential_doesNotSetPaymentOptionsWhenNonPaymentCredential() {
        Assume.assumeFalse(BuildConfig.ENABLE_ASSERTS);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        String clientDataJson = "fakeClientDataJson";
        // Mock ClientDataJsonImplJni to verify the call arguments below.
        ClientDataJsonImplJni.setInstanceForTesting(mClientDataJsonImplMock);
        when(mClientDataJsonImplMock.buildClientDataJson(
                        anyInt(), any(), any(), anyBoolean(), any(), any(), any()))
                .thenReturn(clientDataJson);
        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mCreationOptions.isPaymentCredentialCreation = false;
        // Set AuthenticatorSelection to resident key discouraged, so that the Fido2ApiCallHelper is
        // called instead of CredManHelper.
        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                payment,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();

        Mockito.verify(mClientDataJsonImplMock, Mockito.times(1))
                .buildClientDataJson(
                        eq(ClientDataRequestType.WEB_AUTHN_CREATE),
                        eq(Fido2CredentialRequest.convertOriginToString(mOrigin)),
                        eq(mCreationOptions.challenge),
                        /* isCrossOrigin= */ eq(false),
                        /* optionsByteBuffer= */ isNull(),
                        eq(mCreationOptions.relyingParty.name),
                        eq(mOrigin));
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Assert.assertEquals(
                new String(
                        mCallback.getMakeCredentialResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
    }

    @Test
    @SmallTest
    @UseMethodParameter(SameOriginTestParams.class)
    public void testMakeCredential_remoteDesktopClientOverride_generatesCorrectClientDataJson(
            boolean sameOriginWithAncestors) {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        // Mock ClientDataJsonImplJni to capture the call arguments.
        ClientDataJsonImplJni.setInstanceForTesting(mClientDataJsonImplMock);
        String clientDataJson = "fakeClientDataJson";
        when(mClientDataJsonImplMock.buildClientDataJson(
                        anyInt(), any(), any(), anyBoolean(), any(), any(), any()))
                .thenReturn(clientDataJson);

        // Set a remote desktop client override origin.
        org.chromium.url.internal.mojom.Origin remoteDesktopClientOverrideOriginMojom =
                new org.chromium.url.internal.mojom.Origin();
        remoteDesktopClientOverrideOriginMojom.scheme = "https";
        remoteDesktopClientOverrideOriginMojom.host = "remotedesktop.example.com";
        remoteDesktopClientOverrideOriginMojom.port = 443;
        Origin remoteDesktopClientOverrideOrigin =
                new Origin(remoteDesktopClientOverrideOriginMojom);
        mCreationOptions.remoteDesktopClientOverride = new RemoteDesktopClientOverride();
        mCreationOptions.remoteDesktopClientOverride.origin =
                remoteDesktopClientOverrideOriginMojom;
        mCreationOptions.remoteDesktopClientOverride.sameOriginWithAncestors =
                sameOriginWithAncestors;

        mFrameHost.setLastCommittedUrl(new GURL("https://www.example.com"));
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                /* paymentOptions= */ null,
                mCallback::onRegisterResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();

        Mockito.verify(mClientDataJsonImplMock, Mockito.times(1))
                .buildClientDataJson(
                        eq(ClientDataRequestType.WEB_AUTHN_CREATE),
                        // Verify that the origin used in buildClientDataJson is the
                        // remote desktop client override origin, not the original origin.
                        eq(
                                Fido2CredentialRequest.convertOriginToString(
                                        remoteDesktopClientOverrideOrigin)),
                        eq(mCreationOptions.challenge),
                        /* isCrossOrigin= */ eq(!sameOriginWithAncestors),
                        /* optionsByteBuffer= */ isNull(),
                        eq(mCreationOptions.relyingParty.name),
                        eq(mOrigin));

        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Assert.assertEquals(
                new String(
                        mCallback.getMakeCredentialResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
    }

    @Test
    @SmallTest
    @UseMethodParameter(SameOriginTestParams.class)
    public void testGetAssertion_remoteDesktopClientOverride_generatesCorrectClientDataJson(
            boolean sameOriginWithAncestors) {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        // Mock ClientDataJsonImplJni to capture the call arguments.
        ClientDataJsonImplJni.setInstanceForTesting(mClientDataJsonImplMock);
        String clientDataJson = "fakeClientDataJson";
        when(mClientDataJsonImplMock.buildClientDataJson(
                        anyInt(), any(), any(), anyBoolean(), any(), any(), any()))
                .thenReturn(clientDataJson);

        // Set a remote desktop client override origin.
        org.chromium.url.internal.mojom.Origin remoteDesktopClientOverrideOriginMojom =
                new org.chromium.url.internal.mojom.Origin();
        remoteDesktopClientOverrideOriginMojom.scheme = "https";
        remoteDesktopClientOverrideOriginMojom.host = "remotedesktop.example.com";
        remoteDesktopClientOverrideOriginMojom.port = 443;
        Origin remoteDesktopClientOverrideOrigin =
                new Origin(remoteDesktopClientOverrideOriginMojom);
        mRequestOptions.publicKey.extensions.remoteDesktopClientOverride =
                new RemoteDesktopClientOverride();
        mRequestOptions.publicKey.extensions.remoteDesktopClientOverride.origin =
                remoteDesktopClientOverrideOriginMojom;
        mRequestOptions.publicKey.extensions.remoteDesktopClientOverride.sameOriginWithAncestors =
                sameOriginWithAncestors;

        mFrameHost.setLastCommittedUrl(new GURL("https://www.example.com"));
        mRequest.handleGetCredentialRequest(
                mRequestOptions,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError,
                mCallback::onRequestOutcome);
        mCallback.blockUntilCalled();

        Mockito.verify(mClientDataJsonImplMock, Mockito.times(1))
                .buildClientDataJson(
                        eq(ClientDataRequestType.WEB_AUTHN_GET),
                        // Verify that the origin used in buildClientDataJson is the
                        // remote desktop client override origin, not the original origin.
                        eq(
                                Fido2CredentialRequest.convertOriginToString(
                                        remoteDesktopClientOverrideOrigin)),
                        eq(mRequestOptions.publicKey.challenge),
                        /* isCrossOrigin= */ eq(!sameOriginWithAncestors),
                        /* optionsByteBuffer= */ isNull(),
                        eq(mRequestOptions.publicKey.relyingPartyId),
                        eq(mOrigin));

        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Assert.assertEquals(
                new String(
                        mCallback.getGetAssertionResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
    }
}
