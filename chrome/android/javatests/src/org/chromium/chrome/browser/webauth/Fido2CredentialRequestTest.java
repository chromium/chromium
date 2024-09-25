// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.mockito.ArgumentMatchers.eq;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;
import android.util.Base64;
import android.util.Log;
import android.util.Pair;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PrfValues;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.CreateConfirmationUiDelegate;
import org.chromium.components.webauthn.Fido2Api;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.FidoIntentSender;
import org.chromium.components.webauthn.GpmBrowserOptionsHelper;
import org.chromium.components.webauthn.InternalAuthenticator;
import org.chromium.components.webauthn.InternalAuthenticatorJni;
import org.chromium.components.webauthn.WebauthnBrowserBridge;
import org.chromium.components.webauthn.WebauthnCredentialDetails;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content.browser.ClientDataJsonImplJni;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

/** Unit tests for {@link Fido2CredentialRequestTest}. */
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

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock ClientDataJsonImpl.Natives mClientDataJsonImplMock;

    private Context mContext;
    private MockIntentSender mIntentSender;
    private EmbeddedTestServer mTestServer;
    private MockAuthenticatorRenderFrameHost mFrameHost;
    private MockWebContents mWebContents;
    private MockBrowserBridge mMockBrowserBridge;
    private MockFido2ApiCallHelper mFido2ApiCallHelper;
    private AuthenticationContextProvider mAuthenticationContextProvider;
    private Origin mOrigin;
    private Bundle mBrowserOptions;
    private InternalAuthenticator.Natives mTestAuthenticatorImplJni;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private static final String FILLER_ERROR_MSG = "Error Error";
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private long mStartTimeMs;

    /**
     * This class constructs the parameters array that is used for testMakeCredential_with_param and
     * testGetAssertion_with_param as input parameters.
     */
    public static class ErrorTestParams implements ParameterProvider {
        private static List<ParameterSet> sErrorTestParams =
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

    private static class MockIntentSender implements FidoIntentSender {
        private Pair<Integer, Intent> mNextResult;
        private boolean mInvokeCallbackImmediately = true;
        private Callback<Pair<Integer, Intent>> mCallback;
        private final ConditionVariable mShowIntentCalled = new ConditionVariable();

        void setNextResult(int responseCode, Intent intent) {
            mNextResult = new Pair(responseCode, intent);
        }

        void setNextResultIntent(Intent intent) {
            setNextResult(Activity.RESULT_OK, intent);
        }

        public void setInvokeCallbackImmediately(boolean invokeImmediately) {
            mInvokeCallbackImmediately = invokeImmediately;
        }

        public void invokeCallback() {
            Pair<Integer, Intent> result = mNextResult;
            Callback<Pair<Integer, Intent>> callback = mCallback;
            mNextResult = null;
            mCallback = null;
            callback.onResult(result);
        }

        public void blockUntilShowIntentCalled() {
            mShowIntentCalled.block();
        }

        @Override
        public boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback) {
            if (mNextResult == null) {
                return false;
            }
            if (mInvokeCallbackImmediately) {
                Pair<Integer, Intent> result = mNextResult;
                mNextResult = null;
                callback.onResult(result);
            } else {
                assert mCallback == null;
                mCallback = callback;
            }
            mShowIntentCalled.open();
            return true;
        }
    }

    private static class MockFido2ApiCallHelper extends Fido2ApiCallHelper {
        private List<WebauthnCredentialDetails> mReturnedCredentialDetails;
        private boolean mInvokeCallbackImmediately = true;
        private OnSuccessListener<List<WebauthnCredentialDetails>> mSuccessCallback;

        @Override
        public void invokeFido2GetCredentials(
                AuthenticationContextProvider authenticationContextProvider,
                String relyingPartyId,
                OnSuccessListener<List<WebauthnCredentialDetails>> successCallback,
                OnFailureListener failureCallback) {
            if (mInvokeCallbackImmediately) {
                successCallback.onSuccess(mReturnedCredentialDetails);
                return;
            }
            mSuccessCallback = successCallback;
        }

        public void setReturnedCredentialDetails(List<WebauthnCredentialDetails> details) {
            mReturnedCredentialDetails = details;
        }

        public void setInvokeCallbackImmediately(boolean invokeImmediately) {
            mInvokeCallbackImmediately = invokeImmediately;
        }

        public void invokeSuccessCallback() {
            mSuccessCallback.onSuccess(mReturnedCredentialDetails);
        }
    }

    private static class TestAuthenticatorImplJni implements InternalAuthenticator.Natives {
        private Fido2ApiTestHelper.AuthenticatorCallback mCallback;

        TestAuthenticatorImplJni(Fido2ApiTestHelper.AuthenticatorCallback callback) {
            mCallback = callback;
        }

        @Override
        public void invokeMakeCredentialResponse(
                long nativeInternalAuthenticator, int status, ByteBuffer byteBuffer) {
            mCallback.onRegisterResponse(
                    status,
                    byteBuffer == null
                            ? null
                            : MakeCredentialAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeGetAssertionResponse(
                long nativeInternalAuthenticator, int status, ByteBuffer byteBuffer) {
            mCallback.onSignResponse(
                    status,
                    byteBuffer == null
                            ? null
                            : GetAssertionAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                long nativeInternalAuthenticator, boolean isUVPAA) {}

        @Override
        public void invokeGetMatchingCredentialIdsResponse(
                long nativeInternalAuthenticator, byte[][] matchingCredentials) {}
    }

    private static class MockBrowserBridge extends WebauthnBrowserBridge {
        public enum CallbackInvocationType {
            IMMEDIATE_GET_ASSERTION,
            IMMEDIATE_HYBRID,
            DELAYED
        }

        private byte[] mSelectedCredentialId;
        private List<WebauthnCredentialDetails> mExpectedCredentialList;
        private CallbackInvocationType mInvokeCallbackImmediately =
                CallbackInvocationType.IMMEDIATE_GET_ASSERTION;
        private Callback<byte[]> mGetAssertionCallback;
        private Runnable mHybridCallback;
        private int mCleanupCalled;

        @Override
        public void onCredentialsDetailsListReceived(
                RenderFrameHost frameHost,
                List<WebauthnCredentialDetails> credentialList,
                boolean isConditionalRequest,
                Callback<byte[]> getAssertionCallback,
                Runnable hybridCallback) {
            Assert.assertEquals(mExpectedCredentialList.size(), credentialList.size());
            for (int i = 0; i < credentialList.size(); i++) {
                Assert.assertEquals(
                        mExpectedCredentialList.get(0).mUserName, credentialList.get(0).mUserName);
                Assert.assertEquals(
                        mExpectedCredentialList.get(0).mUserDisplayName,
                        credentialList.get(0).mUserDisplayName);
                Assert.assertArrayEquals(
                        mExpectedCredentialList.get(0).mCredentialId,
                        credentialList.get(0).mCredentialId);
                Assert.assertArrayEquals(
                        mExpectedCredentialList.get(0).mUserId, credentialList.get(0).mUserId);
            }

            mGetAssertionCallback = getAssertionCallback;
            mHybridCallback = hybridCallback;

            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_GET_ASSERTION) {
                invokeGetAssertionCallback();
            }

            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_HYBRID) {
                invokeHybridCallback();
            }
        }

        @Override
        public void cleanupRequest(RenderFrameHost frameHost) {
            mCleanupCalled++;
        }

        public void setSelectedCredentialId(byte[] credentialId) {
            mSelectedCredentialId = credentialId;
        }

        public void setExpectedCredentialDetailsList(
                List<WebauthnCredentialDetails> credentialList) {
            mExpectedCredentialList = credentialList;
        }

        public void setInvokeCallbackImmediately(CallbackInvocationType type) {
            mInvokeCallbackImmediately = type;
        }

        public void invokeGetAssertionCallback() {
            if (mSelectedCredentialId != null) {
                mGetAssertionCallback.onResult(mSelectedCredentialId);
                return;
            }

            /* An empty credential list can occur if a device has no applicable credentials.
             * In production this is passed to native code and the callback will never be
             * invoked. For the sake of testing we invoke with an empty credential selection,
             * which normally would imply an error response.
             */
            if (mExpectedCredentialList.isEmpty()) {
                mGetAssertionCallback.onResult(new byte[0]);
                return;
            }

            /* Return the first ID in the list if one has not been explicitly set. */
            mGetAssertionCallback.onResult(mExpectedCredentialList.get(0).mCredentialId);
        }

        public void invokeHybridCallback() {
            mHybridCallback.run();
        }

        public int getCleanupCalledCount() {
            return mCleanupCalled;
        }
    }

    private static class MockAuthenticatorRenderFrameHost extends MockRenderFrameHost {
        private GURL mLastUrl;
        private boolean mIsPaymentCredentialCreation;

        MockAuthenticatorRenderFrameHost() {}

        @Override
        public GURL getLastCommittedURL() {
            return mLastUrl;
        }

        @Override
        public Origin getLastCommittedOrigin() {
            return Origin.create(mLastUrl);
        }

        public void setLastCommittedURL(GURL url) {
            mLastUrl = url;
        }

        @Override
        public void performMakeCredentialWebAuthSecurityChecks(
                String relyingPartyId,
                Origin effectiveOrigin,
                boolean isPaymentCredentialCreation,
                Callback<WebAuthSecurityChecksResults> callback) {
            mIsPaymentCredentialCreation = isPaymentCredentialCreation;
            super.performMakeCredentialWebAuthSecurityChecks(
                    relyingPartyId, effectiveOrigin, isPaymentCredentialCreation, callback);
        }
    }

    private static final String FIDO_OVERRIDE_COMMAND =
            "su root am broadcast -a com.google.android.gms.phenotype.FLAG_OVERRIDE --es package"
                    + " com.google.android.gms.fido --es user * --esa flags"
                    + " Fido2ApiKnownBrowsers__fingerprints --esa values"
                    + " %s --esa types"
                    + " string --ez commit true --user 0 com.google.android.gms";

    @Before
    public void setUp() throws Exception {
        mContext = ContextUtils.getApplicationContext();

        String fingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(mContext.getPackageName())
                        .stream()
                        .map(s -> s.replaceAll(":", ""))
                        .collect(Collectors.joining("\'"));
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .executeShellCommand(String.format(FIDO_OVERRIDE_COMMAND, fingerprints));

        Log.d(
                TAG,
                "Executing command: '" + String.format(FIDO_OVERRIDE_COMMAND, fingerprints) + "'");
        mIntentSender = new MockIntentSender();
        mTestServer = sActivityTestRule.getTestServer();
        mCallback = Fido2ApiTestHelper.getAuthenticatorCallback();
        String url =
                mTestServer.getURLWithHostName(
                        "subdomain.example.test", "/content/test/data/android/authenticator.html");
        GURL gurl = new GURL(url);
        mOrigin = Origin.create(gurl);
        mBrowserOptions = GpmBrowserOptionsHelper.createDefaultBrowserOptions();
        GpmBrowserOptionsHelper.setIsIncognitoExtraUntilTearDown(false);
        sActivityTestRule.loadUrl(url);
        mFrameHost = new MockAuthenticatorRenderFrameHost();
        mFrameHost.setLastCommittedURL(gurl);
        mWebContents = new MockWebContents();
        mWebContents.renderFrameHost = mFrameHost;

        MockitoAnnotations.initMocks(this);
        mTestAuthenticatorImplJni = new TestAuthenticatorImplJni(mCallback);
        mMocker.mock(InternalAuthenticatorJni.TEST_HOOKS, mTestAuthenticatorImplJni);

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
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

        mFido2ApiCallHelper = new MockFido2ApiCallHelper();
        mFido2ApiCallHelper.setReturnedCredentialDetails(
                Arrays.asList(Fido2ApiTestHelper.getCredentialDetails()));
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        mMockBrowserBridge = new MockBrowserBridge();
        mRequest.overrideBrowserBridgeForTesting(mMockBrowserBridge);

        mStartTimeMs = SystemClock.elapsedRealtime();
    }

    @Test
    @SmallTest
    public void testConvertOriginToString_defaultPortRemoved() {
        Origin origin = Origin.create(new GURL("https://www.example.com:443"));
        String parsedOrigin = Fido2CredentialRequest.convertOriginToString(origin);
        Assert.assertEquals(parsedOrigin, "https://www.example.com/");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testMakeCredential_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testPasskeyMakeCredential_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulPasskeyMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
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
        Assert.assertEquals(response.authenticatorAttachment, AuthenticatorAttachment.PLATFORM);
    }

    @Test
    @SmallTest
    public void testMakeCredential_unsuccessfulAttemptToShowCancelableIntent() {
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultUnknown() {
        mIntentSender.setNextResult(
                Activity.RESULT_FIRST_USER,
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ALGORITHM_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
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

        mRequest.handleMakeCredentialRequest(
                customOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testMakeCredential_noExcludeCredentials() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.excludeCredentials = new PublicKeyCredentialDescriptor[0];
        mRequest.handleMakeCredentialRequest(
                customOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testAuthenticatorImplMakeCredential_success() {
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
    @DisabledTest(message = "crbug.com/347310677")
    public void testAuthenticatorImplMakeCredential_withConfirmationUi_success() {
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
    @DisabledTest(message = "crbug.com/347310677")
    public void testInternalAuthenticatorMakeCredential_success() {
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
    public void testInternalAuthenticatorMakeCredential_attestationIncluded() {
        // This test can't work on Android N because it lacks the java.nio.file
        // APIs used to load the test data.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }

        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithAttestation());

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        assertHasAttestation(response);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_rkRequired_attestationKept()
            throws Exception {
        // This test can't work on Android N because it lacks the java.nio.file
        // APIs used to load the test data.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }

        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithAttestation());

        // Set the residentKey option to trigger attestation removal.
        PublicKeyCredentialCreationOptions creationOptions =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        creationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.REQUIRED;

        mRequest.handleMakeCredentialRequest(
                creationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        assertHasAttestation(response);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_rkPreferred_attestationKept()
            throws Exception {
        // This test can't work on Android N because it lacks the java.nio.file
        // APIs used to load the test data.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }

        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithAttestation());

        // Set the residentKey option to trigger attestation removal.
        PublicKeyCredentialCreationOptions creationOptions =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        creationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.PREFERRED;

        mRequest.handleMakeCredentialRequest(
                creationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        assertHasAttestation(response);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_credprops() throws Exception {
        // This test can't work on Android N because it lacks the java.nio.file
        // APIs used to load the test data.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }

        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntentWithCredProps());

        PublicKeyCredentialCreationOptions creationOptions =
                Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        creationOptions.credProps = true;

        mRequest.handleMakeCredentialRequest(
                creationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        MakeCredentialAuthenticatorResponse response = mCallback.getMakeCredentialResponse();
        Assert.assertTrue(response.echoCredProps);
        Assert.assertTrue(response.hasCredPropsRk);
        Assert.assertTrue(response.credPropsRk);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertionWithoutUvmRequested_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertionWithUvmRequestedWithoutUvmResponded_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequestOptions.extensions.userVerificationMethods = true;
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertionWithUvmRequestedWithUvmResponded_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        mRequestOptions.extensions.userVerificationMethods = true;
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Assert.assertTrue(response.extensions.echoUserVerificationMethods);
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithUserId_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUserId());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Assert.assertArrayEquals(response.userHandle, Fido2ApiTestHelper.TEST_USER_HANDLE);
    }

    @Test
    @SmallTest
    public void testGetAssertion_unsuccessfulAttemptToShowCancelableIntent() {
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_missingExtra() {
        // An intent missing FIDO2_KEY_RESPONSE_EXTRA.
        mIntentSender.setNextResultIntent(new Intent());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_nullIntent() {
        // Don't set an intent to be returned at all.
        mIntentSender.setNextResultIntent(null);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultCanceled() {
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultUnknown() {
        mIntentSender.setNextResult(
                Activity.RESULT_FIRST_USER,
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
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

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_appIdUsed() {
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.extensions.appid = "www.example.com";
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Assert.assertEquals(response.extensions.echoAppidExtension, true);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testAuthenticatorImplGetAssertionWithUvmRequestedWithUvmResponded_success() {
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
        mRequestOptions.extensions.userVerificationMethods = true;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getAssertion(
                            mRequestOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onSignResponse(status, response));
                });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
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
        mRequestOptions.extensions.prf = true;
        mRequestOptions.extensions.prfInputs =
                new PrfValues[] {
                    prfValues,
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getAssertion(
                            mRequestOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onSignResponse(status, response));
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
    public void testAuthenticatorImplGetAssertion_resultCanceled() {
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
                    authenticator.getAssertion(
                            mRequestOptions,
                            (status, response, dom_exception) ->
                                    mCallback.onSignResponse(status, response));
                });
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        authenticator.close();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testInternalAuthenticatorGetAssertionWithUvmRequestedWithUvmResponded_success() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(
                        mContext, mIntentSender, mFrameHost, mOrigin);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.extensions.userVerificationMethods = true;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getAssertion(mRequestOptions.serialize());
                });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorGetAssertion_resultCanceled() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(
                        mContext, mIntentSender, mFrameHost, mOrigin);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    authenticator.getAssertion(mRequestOptions.serialize());
                });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testMakeCredential_attestationNone() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.NONE;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testMakeCredential_attestationIndirect() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.INDIRECT;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testMakeCredential_attestationDirect() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.DIRECT;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testMakeCredential_attestationEnterprise() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.ENTERPRISE;
        mRequest.handleMakeCredentialRequest(
                customOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_isPaymentCredentialCreationPassedToFrameHost() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));

        mCreationOptions.isPaymentCredentialCreation = true;
        Assert.assertFalse(mFrameHost.mIsPaymentCredentialCreation);
        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        Assert.assertTrue(mFrameHost.mIsPaymentCredentialCreation);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_emptyAllowCredentials1() {
        // Passes conversion and gets rejected by GmsCore
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.NOT_ALLOWED_ERR,
                        "Authentication request must have non-empty allowList"));

        // Requests with empty allowCredentials are only passed to GMSCore if there are no
        // local passkeys available.
        mFido2ApiCallHelper.setReturnedCredentialDetails(new ArrayList<>());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_emptyAllowCredentials2() {
        // Passes conversion and gets rejected by GmsCore
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.NOT_ALLOWED_ERR,
                        "Request doesn't have a valid list of allowed credentials."));

        // Requests with empty allowCredentials are only passed to GMSCore if there are no
        // local passkeys available.
        mFido2ApiCallHelper.setReturnedCredentialDetails(new ArrayList<>());

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_constraintErrorNoScreenlock() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(
                        Fido2Api.CONSTRAINT_ERR, "The device is not secured with any screen lock"));

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testMakeCredential_with_param(Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(errorCode, errorMsg));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testGetAssertion_with_param(Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(errorCode, errorMsg));

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
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
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testGetAssertion_with_param_nullErrorMessage(
            Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(errorCode, null));

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
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
        Fido2ApiTestHelper.mockClientDataJson(mMocker, clientDataJson);

        mFrameHost.setLastCommittedURL(new GURL("https://www.chromium.org/pay"));

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.challenge = new byte[3];
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                payment,
                mCallback::onSignResponse,
                mCallback::onError);
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

        Fido2ApiTestHelper.mockClientDataJson(mMocker, null);

        mFrameHost.setLastCommittedURL(new GURL("https://www.chromium.org/pay"));

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.challenge = new byte[3];
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                payment,
                mCallback::onSignResponse,
                mCallback::onError);
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
        mMocker.mock(ClientDataJsonImplJni.TEST_HOOKS, mClientDataJsonImplMock);

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.challenge = new byte[3];
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                topOrigin,
                payment,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);

        ArgumentCaptor<Origin> topOriginCaptor = ArgumentCaptor.forClass(Origin.class);
        Mockito.verify(mClientDataJsonImplMock, Mockito.times(1))
                .buildClientDataJson(
                        eq(ClientDataRequestType.PAYMENT_GET),
                        eq(Fido2CredentialRequest.convertOriginToString(mOrigin)),
                        eq(mRequestOptions.challenge),
                        eq(false),
                        eq(payment.serialize()),
                        eq(mRequestOptions.relyingPartyId),
                        topOriginCaptor.capture());

        String topOriginString =
                Fido2CredentialRequest.convertOriginToString(topOriginCaptor.getValue());
        String expectedTopOriginString = Fido2CredentialRequest.convertOriginToString(topOrigin);
        Assert.assertEquals(expectedTopOriginString, topOriginString);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_emptyAllowCredentials_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_emptyAllowCredentialsUserCancels_notAllowedError() {
        mMockBrowserBridge.setSelectedCredentialId(new byte[0]);
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUi_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUi_failureEmptyCredential() {
        mMockBrowserBridge.setSelectedCredentialId(new byte[0]);
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiNondiscoverableCredential_failure() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        WebauthnCredentialDetails nonDiscoverableCredDetails =
                Fido2ApiTestHelper.getCredentialDetails();
        nonDiscoverableCredDetails.mIsDiscoverable = false;
        mFido2ApiCallHelper.setReturnedCredentialDetails(Arrays.asList(nonDiscoverableCredDetails));
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUi_cancelWhileFetchingCredentials() {
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mFido2ApiCallHelper.setInvokeCallbackImmediately(false);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mRequest.cancelConditionalGetAssertion();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.ABORT_ERROR), mCallback.getStatus());

        // Also validate that when the FIDO getCredentials call is completed, nothing happens.
        // The MockBrowserBridge will assert if onCredentialsDetailsListReceived is called.
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());
        mFido2ApiCallHelper.invokeSuccessCallback();
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 0);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUi_cancelWhileWaitingForSelection() {
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));
        mMockBrowserBridge.setInvokeCallbackImmediately(
                MockBrowserBridge.CallbackInvocationType.DELAYED);
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mRequest.cancelConditionalGetAssertion();

        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.ABORT_ERROR), mCallback.getStatus());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiCancelWhileRequestSentToPlatform_ignored() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mIntentSender.setInvokeCallbackImmediately(false);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mIntentSender.blockUntilShowIntentCalled();
        mRequest.cancelConditionalGetAssertion();
        mIntentSender.invokeCallback();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiCancelWhileRequestSentToPlatformUserDeny_cancelled() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR, ""));
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mIntentSender.setInvokeCallbackImmediately(false);

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mIntentSender.blockUntilShowIntentCalled();
        mRequest.cancelConditionalGetAssertion();
        mIntentSender.invokeCallback();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.ABORT_ERROR), mCallback.getStatus());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiRequestSentToPlatformUserDeny_doesNotComplete() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR, ""));
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mIntentSender.blockUntilShowIntentCalled();
        // Null status indicates the callback was not invoked.
        Assert.assertNull(mCallback.getStatus());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 0);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiRetryAfterUserDeny_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR, ""));
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mIntentSender.blockUntilShowIntentCalled();
        // Null status indicates the callback was not invoked.
        Assert.assertNull(mCallback.getStatus());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 0);

        // Select credential again, and provide a success response this time.
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.invokeGetAssertionCallback();
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiWithAllowCredentialMatch_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(
                        new WebauthnCredentialDetails[] {
                            Fido2ApiTestHelper.getCredentialDetails()
                        }));

        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiWithAllowCredentialMismatch_failure() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {3, 2, 1};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
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
    @DisabledTest(message = "crbug.com/347310677")
    public void testGetAssertion_conditionalUiHybrid_success() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        FeatureList.setTestValues(testValues);
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mMockBrowserBridge.setExpectedCredentialDetailsList(
                Arrays.asList(Fido2ApiTestHelper.getCredentialDetails()));
        mMockBrowserBridge.setInvokeCallbackImmediately(
                MockBrowserBridge.CallbackInvocationType.IMMEDIATE_HYBRID);

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
        Assert.assertEquals(mMockBrowserBridge.getCleanupCalledCount(), 1);
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

    private void assertHasAttestation(MakeCredentialAuthenticatorResponse response) {
        // Since the CBOR must be in canonical form, the "fmt" key comes first
        // and we can match against "fmt=android-safetynet".
        final byte[] expectedPrefix =
                new byte[] {
                    0xa3 - 256,
                    0x63,
                    0x66,
                    0x6d,
                    0x74,
                    0x71,
                    0x61,
                    0x6e,
                    0x64,
                    0x72,
                    0x6f,
                    0x69,
                    0x64,
                    0x2d,
                    0x73,
                    0x61,
                    0x66,
                    0x65,
                    0x74,
                    0x79,
                    0x6e,
                    0x65,
                    0x74
                };
        byte[] actualPrefix =
                Arrays.copyOfRange(response.attestationObject, 0, expectedPrefix.length);
        Assert.assertArrayEquals(expectedPrefix, actualPrefix);
    }

    @Test
    @SmallTest
    public void testGetAssertion_generatesClientDataJson() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        String clientDataJson = "520";
        Fido2ApiTestHelper.mockClientDataJson(mMocker, clientDataJson);

        mFrameHost.setLastCommittedURL(new GURL("https://www.example.com"));

        mRequestOptions.challenge = new byte[3];
        mRequest.handleGetAssertionRequest(
                mRequestOptions,
                /* maybeClientDataHash= */ null,
                mOrigin,
                mOrigin,
                /* payment= */ null,
                mCallback::onSignResponse,
                mCallback::onError);
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
        Fido2ApiTestHelper.mockClientDataJson(mMocker, clientDataJson);

        mFrameHost.setLastCommittedURL(new GURL("https://www.example.com"));

        mRequest.handleMakeCredentialRequest(
                mCreationOptions,
                /* maybeClientDataHash= */ null,
                mBrowserOptions,
                mOrigin,
                mOrigin,
                mCallback::onRegisterResponse,
                mCallback::onError);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertEquals(
                new String(
                        mCallback.getMakeCredentialResponse().info.clientDataJson,
                        StandardCharsets.UTF_8),
                clientDataJson);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }
}
