// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.ConditionVariable;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;
import android.util.Base64;
import android.util.Log;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.Fido2Api;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.InternalAuthenticator;
import org.chromium.components.webauthn.InternalAuthenticatorJni;
import org.chromium.components.webauthn.WebAuthnBrowserBridge;
import org.chromium.components.webauthn.WebAuthnCredentialDetails;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebAuthenticationDelegate;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link Fido2CredentialRequestTest}.
 */

@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "ignore-certificate-errors",
})
@Batch(Batch.PER_CLASS)
public class Fido2CredentialRequestTest {
    private static final String TAG = "Fido2CredentialRequestTest";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);
    @Rule
    public JniMocker mMocker = new JniMocker();

    private MockIntentSender mIntentSender;
    private EmbeddedTestServer mTestServer;
    private MockAuthenticatorRenderFrameHost mFrameHost;
    private MockBrowserBridge mMockBrowserBridge;
    private MockFido2ApiCallHelper mFido2ApiCallHelper;
    private Origin mOrigin;
    private InternalAuthenticator.Natives mTestAuthenticatorImplJni;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";
    private static final String FILLER_ERROR_MSG = "Error Error";
    private AuthenticatorCallback mCallback;
    private long mStartTimeMs;
    private MockWebContents mMockWebContents;

    /**
     * This class constructs the parameters array that is used for testMakeCredential_with_param and
     * testGetAssertion_with_param as input parameters.
     */
    public static class ErrorTestParams implements ParameterProvider {
        private static List<ParameterSet> sErrorTestParams = Arrays.asList(
                new ParameterSet()
                        .value(Fido2Api.SECURITY_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.INVALID_DOMAIN))
                        .name("securityError"),
                new ParameterSet()
                        .value(Fido2Api.TIMEOUT_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR))
                        .name("timeoutError"),
                new ParameterSet()
                        .value(Fido2Api.ENCODING_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                        .name("encodingError"),
                new ParameterSet()
                        .value(Fido2Api.NOT_ALLOWED_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR))
                        .name("notAllowedError"),
                new ParameterSet()
                        .value(Fido2Api.DATA_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR))
                        .name("dataError"),
                new ParameterSet()
                        .value(Fido2Api.NOT_SUPPORTED_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR))
                        .name("notSupportedError"),
                new ParameterSet()
                        .value(Fido2Api.CONSTRAINT_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED))
                        .name("constraintErrorReRegistration"),
                new ParameterSet()
                        .value(Fido2Api.INVALID_STATE_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                        .name("invalidStateError"),
                new ParameterSet()
                        .value(Fido2Api.UNKNOWN_ERR, FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                        .name("unknownError"));
        @Override
        public List<ParameterSet> getParameters() {
            return sErrorTestParams;
        }
    }

    private static class MockIntentSender implements WebAuthenticationDelegate.IntentSender {
        private Pair<Integer, Intent> mNextResult;

        void setNextResult(int responseCode, Intent intent) {
            mNextResult = new Pair(responseCode, intent);
        }

        void setNextResultIntent(Intent intent) {
            setNextResult(Activity.RESULT_OK, intent);
        }

        @Override
        public boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback) {
            if (mNextResult == null) {
                return false;
            }

            Pair<Integer, Intent> result = mNextResult;
            mNextResult = null;
            callback.onResult(result);
            return true;
        }
    }

    private static class MockFido2ApiCallHelper extends Fido2ApiCallHelper {
        private WebAuthnCredentialDetails mReturnedCredentialDetails;

        @Override
        public void invokeFido2GetCredentials(String relyingPartyId, int supportLevel,
                OnSuccessListener<List<WebAuthnCredentialDetails>> successCallback,
                OnFailureListener failureCallback) {
            successCallback.onSuccess(
                    Arrays.asList(new WebAuthnCredentialDetails[] {mReturnedCredentialDetails}));
        }

        public void setReturnedCredentialDetails(WebAuthnCredentialDetails details) {
            mReturnedCredentialDetails = details;
        }
    }

    private static class AuthenticatorCallback {
        private Integer mStatus;
        private MakeCredentialAuthenticatorResponse mMakeCredentialResponse;
        private GetAssertionAuthenticatorResponse mGetAssertionAuthenticatorResponse;

        // Signals when request is complete.
        private final ConditionVariable mDone = new ConditionVariable();

        AuthenticatorCallback() {}

        public void onRegisterResponse(
                Integer status, MakeCredentialAuthenticatorResponse response) {
            assert mStatus == null;
            mStatus = status;
            mMakeCredentialResponse = response;
            unblock();
        }

        public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response) {
            assert mStatus == null;
            mStatus = status;
            mGetAssertionAuthenticatorResponse = response;
            unblock();
        }

        public void onError(Integer status) {
            assert mStatus == null;
            mStatus = status;
            unblock();
        }

        public Integer getStatus() {
            return mStatus;
        }

        public MakeCredentialAuthenticatorResponse getMakeCredentialResponse() {
            return mMakeCredentialResponse;
        }

        public GetAssertionAuthenticatorResponse getGetAssertionResponse() {
            return mGetAssertionAuthenticatorResponse;
        }

        public void blockUntilCalled() {
            mDone.block();
        }

        private void unblock() {
            mDone.open();
        }
    }

    private static class TestAuthenticatorImplJni implements InternalAuthenticator.Natives {
        private AuthenticatorCallback mCallback;

        TestAuthenticatorImplJni(AuthenticatorCallback callback) {
            mCallback = callback;
        }

        @Override
        public void invokeMakeCredentialResponse(
                long nativeInternalAuthenticator, int status, ByteBuffer byteBuffer) {
            mCallback.onRegisterResponse(status,
                    byteBuffer == null
                            ? null
                            : MakeCredentialAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeGetAssertionResponse(
                long nativeInternalAuthenticator, int status, ByteBuffer byteBuffer) {
            mCallback.onSignResponse(status,
                    byteBuffer == null ? null
                                       : GetAssertionAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                long nativeInternalAuthenticator, boolean isUVPAA) {}
    }

    private static class MockOrigin extends Origin {
        private final String mScheme;
        private final String mHost;
        private final int mPort;

        public MockOrigin(GURL url) {
            mScheme = url.getScheme();
            mHost = url.getHost();
            String portStr = url.getPort();
            mPort = portStr.isEmpty() ? 443 : Integer.parseInt(portStr);
        }

        public MockOrigin(String scheme, String host, int port) {
            mScheme = scheme;
            mHost = host;
            mPort = port;
        }

        @Override
        public String getScheme() {
            return mScheme;
        }

        @Override
        public String getHost() {
            return mHost;
        }

        @Override
        public int getPort() {
            return mPort;
        }

        @Override
        public boolean isOpaque() {
            return false;
        }
    }

    private static class MockBrowserBridge extends WebAuthnBrowserBridge {
        private byte[] mSelectedCredentialId;
        private List<WebAuthnCredentialDetails> mExpectedCredentialList;

        @Override
        public void onCredentialsDetailsListReceived(RenderFrameHost frameHost,
                List<WebAuthnCredentialDetails> credentialList, Callback<byte[]> callback) {
            Assert.assertEquals(mExpectedCredentialList.size(), credentialList.size());
            for (int i = 0; i < credentialList.size(); i++) {
                Assert.assertEquals(
                        mExpectedCredentialList.get(0).mUserName, credentialList.get(0).mUserName);
                Assert.assertEquals(mExpectedCredentialList.get(0).mUserDisplayName,
                        credentialList.get(0).mUserDisplayName);
                Assert.assertArrayEquals(mExpectedCredentialList.get(0).mCredentialId,
                        credentialList.get(0).mCredentialId);
                Assert.assertArrayEquals(
                        mExpectedCredentialList.get(0).mUserId, credentialList.get(0).mUserId);
            }

            if (mSelectedCredentialId != null) {
                callback.onResult(mSelectedCredentialId);
                return;
            }

            /* An empty credential list can occur if a device has no applicable credentials.
             * In production this is passed to native code and the callback will never be
             * invoked. For the sake of testing we invoke with an empty credential selection,
             * which normally would imply an error response.
             */
            if (credentialList.isEmpty()) {
                callback.onResult(new byte[0]);
                return;
            }

            /* Return the first ID in the list if one has not been explicitly set. */
            callback.onResult(credentialList.get(0).mCredentialId);
        }

        public void setSelectedCredentialId(byte[] credentialId) {
            mSelectedCredentialId = credentialId;
        }

        public void setExpectedCredentialDetailsList(
                List<WebAuthnCredentialDetails> credentialList) {
            mExpectedCredentialList = credentialList;
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
            return new MockOrigin(mLastUrl);
        }

        public void setLastCommittedURL(GURL url) {
            mLastUrl = url;
        }

        @Override
        public int performMakeCredentialWebAuthSecurityChecks(String relyingPartyId,
                Origin effectiveOrigin, boolean isPaymentCredentialCreation) {
            super.performMakeCredentialWebAuthSecurityChecks(
                    relyingPartyId, effectiveOrigin, isPaymentCredentialCreation);
            mIsPaymentCredentialCreation = isPaymentCredentialCreation;
            return 0;
        }
    }

    @Before
    public void setUp() throws Exception {
        Assume.assumeTrue(gmsVersionSupported());
        mIntentSender = new MockIntentSender();
        mTestServer = sActivityTestRule.getTestServer();
        mCallback = new AuthenticatorCallback();
        String url = mTestServer.getURLWithHostName(
                "subdomain.example.test", "/content/test/data/android/authenticator.html");
        GURL gurl = new GURL(url);
        mOrigin = new MockOrigin(gurl);
        sActivityTestRule.loadUrl(url);
        mFrameHost = new MockAuthenticatorRenderFrameHost();
        mFrameHost.setLastCommittedURL(gurl);
        mMockWebContents = new MockWebContents();

        MockitoAnnotations.initMocks(this);
        mTestAuthenticatorImplJni = new TestAuthenticatorImplJni(mCallback);
        mMocker.mock(InternalAuthenticatorJni.TEST_HOOKS, mTestAuthenticatorImplJni);

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
        mRequest = new Fido2CredentialRequest(
                mIntentSender, WebAuthenticationDelegate.Support.BROWSER);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(mRequest);

        mFido2ApiCallHelper = new MockFido2ApiCallHelper();
        mFido2ApiCallHelper.setReturnedCredentialDetails(Fido2ApiTestHelper.getCredentialDetails());
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        mMockBrowserBridge = new MockBrowserBridge();
        mRequest.overrideBrowserBridgeForTesting(mMockBrowserBridge);

        mRequest.setWebContentsForTesting(mMockWebContents);
        mStartTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * Used to enable early exit of tests on bots that don't support GmsCore v16.1+
     */
    private boolean gmsVersionSupported() {
        if (PackageUtils.getPackageVersion(
                    ContextUtils.getApplicationContext(), GOOGLE_PLAY_SERVICES_PACKAGE)
                >= AuthenticatorImpl.GMSCORE_MIN_VERSION) {
            return true;
        }
        return false;
    }

    @Test
    @SmallTest
    public void testConvertOriginToString_defaultPortRemoved() {
        MockOrigin origin = new MockOrigin("https", "www.example.com", 443);
        String parsedOrigin = mRequest.convertOriginToString(origin);
        Assert.assertEquals(parsedOrigin, "https://www.example.com/");
    }

    @Test
    @SmallTest
    public void testMakeCredential_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_unsuccessfulAttemptToShowCancelableIntent() {
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultCanceled() {
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultUnknown() {
        mIntentSender.setNextResult(Activity.RESULT_FIRST_USER,
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_nullRpIcon() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.relyingParty.icon = null;
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_nullUserIcon() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.user.icon = null;
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
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

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ALGORITHM_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_noEligibleParameters2() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.type = 10; // Not a valid type.
        customOptions.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ALGORITHM_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_parametersContainEligibleAndNoneligible() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = 1; // Not a valid algorithm identifier.
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        PublicKeyCredentialParameters[] multiParams = new PublicKeyCredentialParameters[] {
                customOptions.publicKeyParameters[0], parameters};
        customOptions.publicKeyParameters = multiParams;
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.excludeCredentials = null;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_success() {
        AuthenticatorImpl authenticator = new AuthenticatorImpl(
                mIntentSender, mFrameHost, WebAuthenticationDelegate.Support.BROWSER);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            authenticator.makeCredential(mCreationOptions,
                    (status, response,
                            dom_exception) -> mCallback.onRegisterResponse(status, response));
        });

        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_resultCanceled() {
        AuthenticatorImpl authenticator = new AuthenticatorImpl(
                mIntentSender, mFrameHost, WebAuthenticationDelegate.Support.BROWSER);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            authenticator.makeCredential(mCreationOptions,
                    (status, response,
                            dom_exception) -> mCallback.onRegisterResponse(status, response));
        });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_success() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(mIntentSender, mFrameHost);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { authenticator.makeCredential(mCreationOptions.serialize()); });

        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorMakeCredential_resultCanceled() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(mIntentSender, mFrameHost);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { authenticator.makeCredential(mCreationOptions.serialize()); });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithoutUvmRequested_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithUvmRequestedWithoutUvmResponded_success() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequestOptions.userVerificationMethods = true;
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithUvmRequestedWithUvmResponded_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        mRequestOptions.userVerificationMethods = true;
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Assert.assertTrue(response.echoUserVerificationMethods);
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithUserId_success() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUserId());

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Assert.assertArrayEquals(response.userHandle, Fido2ApiTestHelper.TEST_USER_HANDLE);
    }

    @Test
    @SmallTest
    public void testGetAssertion_unsuccessfulAttemptToShowCancelableIntent() {
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultCanceled() {
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultUnknown() {
        mIntentSender.setNextResult(Activity.RESULT_FIRST_USER,
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_unknownErrorCredentialNotRecognized() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(
                Fido2Api.UNKNOWN_ERR, "Low level error 0x6a80"));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_appIdUsed() {
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.appid = "www.example.com";
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Assert.assertEquals(response.echoAppidExtension, true);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertionWithUvmRequestedWithUvmResponded_success() {
        AuthenticatorImpl authenticator = new AuthenticatorImpl(
                mIntentSender, mFrameHost, WebAuthenticationDelegate.Support.BROWSER);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.userVerificationMethods = true;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            authenticator.getAssertion(mRequestOptions,
                    (status, response,
                            dom_exception) -> mCallback.onSignResponse(status, response));
        });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertion_resultCanceled() {
        AuthenticatorImpl authenticator = new AuthenticatorImpl(
                mIntentSender, mFrameHost, WebAuthenticationDelegate.Support.BROWSER);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            authenticator.getAssertion(mRequestOptions,
                    (status, response,
                            dom_exception) -> mCallback.onSignResponse(status, response));
        });
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorGetAssertionWithUvmRequestedWithUvmResponded_success() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(mIntentSender, mFrameHost);
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.userVerificationMethods = true;

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { authenticator.getAssertion(mRequestOptions.serialize()); });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testInternalAuthenticatorGetAssertion_resultCanceled() {
        InternalAuthenticator authenticator =
                InternalAuthenticator.createForTesting(mIntentSender, mFrameHost);
        mIntentSender.setNextResult(Activity.RESULT_CANCELED, null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { authenticator.getAssertion(mRequestOptions.serialize()); });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationNone() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.NONE;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationIndirect() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.INDIRECT;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationDirect() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.DIRECT;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationEnterprise() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.ENTERPRISE;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_invalidStateErrorDuplicateRegistration() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_isPaymentCredentialCreationPassedToFrameHost() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));

        mCreationOptions.isPaymentCredentialCreation = true;
        Assert.assertFalse(mFrameHost.mIsPaymentCredentialCreation);
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertTrue(mFrameHost.mIsPaymentCredentialCreation);
    }

    @Test
    @SmallTest
    public void testGetAssertion_emptyAllowCredentials1() {
        // Passes conversion and gets rejected by GmsCore
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.allowCredentials = null;
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(
                Fido2Api.NOT_ALLOWED_ERR, "Authentication request must have non-empty allowList"));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_emptyAllowCredentials2() {
        // Passes conversion and gets rejected by GmsCore
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.allowCredentials = null;
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(Fido2Api.NOT_ALLOWED_ERR,
                        "Request doesn't have a valid list of allowed credentials."));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_constraintErrorNoScreenlock() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(
                Fido2Api.CONSTRAINT_ERR, "The device is not secured with any screen lock"));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_constraintErrorNoScreenlock() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createErrorIntent(
                Fido2Api.CONSTRAINT_ERR, "The device is not secured with any screen lock"));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testMakeCredential_with_param(Integer errorCode, String errorMsg, Integer status) {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createErrorIntent(errorCode, errorMsg));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_securePaymentConfirmation_canReplaceClientDataJson() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        Fido2ApiTestHelper.setSecurePaymentConfirmationEnabled(mMocker);
        String clientDataJson = "520";
        Fido2ApiTestHelper.mockClientDataJson(mMocker, clientDataJson);

        mMockWebContents.setLastCommittedUrl(new GURL("https://www.chromium.org/pay"));

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.challenge = new byte[3];
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, payment,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertEquals(new String(mCallback.getGetAssertionResponse().info.clientDataJson,
                                    StandardCharsets.UTF_8),
                clientDataJson);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_securePaymentConfirmation_clientDataJsonCannotBeEmpty() {
        mIntentSender.setNextResultIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());

        Fido2ApiTestHelper.setSecurePaymentConfirmationEnabled(mMocker);
        Fido2ApiTestHelper.mockClientDataJson(mMocker, null);

        mMockWebContents.setLastCommittedUrl(new GURL("https://www.chromium.org/pay"));

        PaymentOptions payment = Fido2ApiTestHelper.createPaymentOptions();
        mRequestOptions.challenge = new byte[3];
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, payment,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
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
        mMockBrowserBridge.setExpectedCredentialDetailsList(Arrays.asList(
                new WebAuthnCredentialDetails[] {Fido2ApiTestHelper.getCredentialDetails()}));

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(Integer.valueOf(AuthenticatorStatus.SUCCESS), mCallback.getStatus());
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUi_failureEmptyCredential() {
        mMockBrowserBridge.setSelectedCredentialId(new byte[0]);
        mMockBrowserBridge.setExpectedCredentialDetailsList(Arrays.asList(
                new WebAuthnCredentialDetails[] {Fido2ApiTestHelper.getCredentialDetails()}));
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_conditionalUiNondiscoverableCredential_failure() {
        mIntentSender.setNextResultIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        WebAuthnCredentialDetails nonDiscoverableCredDetails =
                Fido2ApiTestHelper.getCredentialDetails();
        nonDiscoverableCredDetails.mIsDiscoverable = false;
        mFido2ApiCallHelper.setReturnedCredentialDetails(nonDiscoverableCredDetails);
        mMockBrowserBridge.setExpectedCredentialDetailsList(new ArrayList<>());

        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR), mCallback.getStatus());
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
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

        byte encodings[][] = new byte[2][];
        for (int i = 0; i < encodings.length; i++) {
            Parcel parcel = Parcel.obtain();
            parcel.writeInt(16 /* VAL_PARCELABLEARRRAY */);
            if (i > 0) {
                // include length prefix
                final int l = TestParcelable.CLASS_NAME.length();
                parcel.writeInt(4 /* array length */ + 4 /* string length */
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
}
