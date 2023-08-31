// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.webauthn.CredManHelper;
import org.chromium.components.webauthn.CredManMetricsHelper;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManCreateRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManGetRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManPrepareRequestEnum;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.WebAuthnBrowserBridge;
import org.chromium.content_public.browser.RenderFrameHost;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class CredManHelperRobolectricTest {
    private CredManHelper mCredManHelper;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private FakeAndroidCredentialManager mCredentialManager;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private String mOriginString = "https://subdomain.coolwebsitekayserispor.com";
    private byte[] mMaybeClientDataHash = new byte[] {1, 2, 3};

    @Mock
    private Context mContext;
    @Mock
    private RenderFrameHost mFrameHost;
    @Mock
    private CredManMetricsHelper mMetricsHelper;
    @Mock
    private WebAuthnBrowserBridge mBrowserBridge;
    @Mock
    private Callback<Integer> mErrorCallback;

    private CredManHelper.BridgeProvider mBridgeProvider = new CredManHelper.BridgeProvider() {
        @Override
        public WebAuthnBrowserBridge getBridge() {
            return mBrowserBridge;
        }
    };

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Before
    public void setUp() throws Exception {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        MockitoAnnotations.openMocks(this);

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.REQUIRED;
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];

        Fido2ApiTestHelper.mockFido2CredentialRequestJni(mMocker);
        Fido2ApiTestHelper.mockClientDataJson(mMocker, "{}");

        mCallback = Fido2ApiTestHelper.getAuthenticatorCallback();

        mCredentialManager = new FakeAndroidCredentialManager();

        mCredManHelper = new CredManHelper(mBridgeProvider, /*playServicesAvailable=*/true);
        mCredManHelper.setCredManClassesForTesting(mCredentialManager,
                FakeAndroidCredManCreateRequest.Builder.class,
                FakeAndroidCredManGetRequest.Builder.class,
                FakeAndroidCredentialOption.Builder.class, mMetricsHelper);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_default_success() {
        int result = mCredManHelper.startMakeRequest(mContext, mFrameHost, mCreationOptions,
                mOriginString, /*maybeClientDataHash=*/null, mCallback::onRegisterResponse,
                mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        FakeAndroidCredManCreateRequest credManRequest = mCredentialManager.getCreateRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin()).isEqualTo(mOriginString);
        assertThat(credManRequest.getType())
                .isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(credManRequest.getCredentialData().getString(
                           "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_make_request}");
        assertThat(credManRequest.getAlwaysSendAppInfoToProvider()).isTrue();
        assertThat(credManRequest.getCandidateQueryData().containsKey("com.android.chrome.CHANNEL"))
                .isTrue();

        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));

        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SUCCESS);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_withExplicitHash_success() {
        int result = mCredManHelper.startMakeRequest(mContext, mFrameHost, mCreationOptions,
                mOriginString, mMaybeClientDataHash, mCallback::onRegisterResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        FakeAndroidCredManCreateRequest credManRequest = mCredentialManager.getCreateRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin()).isEqualTo(mOriginString);
        assertThat(credManRequest.getCredentialData().getByteArray(
                           "androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(mMaybeClientDataHash);
        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_userCancel_notAllowedError() {
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_USER_CANCELED", "Message"));

        int result = mCredManHelper.startMakeRequest(mContext, mFrameHost, mCreationOptions,
                mOriginString, mMaybeClientDataHash, mCallback::onRegisterResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.CANCELLED);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_invalidStateError_credentialExcluded() {
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                CredManHelper.CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR,
                "Message"));

        int result = mCredManHelper.startMakeRequest(mContext, mFrameHost, mCreationOptions,
                mOriginString, mMaybeClientDataHash, mCallback::onRegisterResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.CREDENTIAL_EXCLUDED);
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SUCCESS);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_unknownError_unknownError() {
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_UNKNOWN", "Message"));

        int result = mCredManHelper.startMakeRequest(mContext, mFrameHost, mCreationOptions,
                mOriginString, mMaybeClientDataHash, mCallback::onRegisterResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.UNKNOWN_ERROR);
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.FAILURE);
    }

    @Test
    @SmallTest
    public void testStartGetRequest_default_success() {
        int result =
                mCredManHelper.startGetRequest(mContext, mFrameHost, mRequestOptions, mOriginString,
                        /*isCrossOrigin=*/false, /*maybeClientDataHash=*/null,
                        mCallback::onSignResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin()).isEqualTo(mOriginString);
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option).isNotNull();
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(option.getCredentialRetrievalData().getString(
                           "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_get_request}");
        assertThat(option.getCandidateQueryData().containsKey("com.android.chrome.CHANNEL"))
                .isTrue();
        assertThat(option.isSystemProviderRequired()).isFalse();
        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));
        verify(mBrowserBridge, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SENT_REQUEST), any());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SUCCESS_PASSKEY), any());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_withExplicitHash_success() {
        int result =
                mCredManHelper.startGetRequest(mContext, mFrameHost, mRequestOptions, mOriginString,
                        /*isCrossOrigin=*/false, mMaybeClientDataHash, mCallback::onSignResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option.getCredentialRetrievalData().getByteArray(
                           "androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(mMaybeClientDataHash);
    }

    @Test
    @SmallTest
    public void testStartGetRequest_noCredentials_noCredentialsFallbackCalled() {
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_NO_CREDENTIAL", "Message"));
        Runnable noCredentialsFallback = Mockito.mock(Runnable.class);

        mCredManHelper.setNoCredentialsFallback(noCredentialsFallback);
        int result =
                mCredManHelper.startGetRequest(mContext, mFrameHost, mRequestOptions, mOriginString,
                        /*isCrossOrigin=*/false, mMaybeClientDataHash, mCallback::onSignResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(
                credManRequest.getData().getBoolean(
                        "androidx.credentials.BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS"))
                .isTrue();
        verify(noCredentialsFallback, times(1)).run();
        verify(mBrowserBridge, times(0)).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_userCancel_notAllowedError() {
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_USER_CANCELED", "Message"));

        int result =
                mCredManHelper.startGetRequest(mContext, mFrameHost, mRequestOptions, mOriginString,
                        /*isCrossOrigin=*/false, mMaybeClientDataHash, mCallback::onSignResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
        verify(mBrowserBridge, times(0)).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.CANCELLED), any());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_unknownError_unknownError() {
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_UNKNOWN", "Message"));

        int result =
                mCredManHelper.startGetRequest(mContext, mFrameHost, mRequestOptions, mOriginString,
                        /*isCrossOrigin=*/false, mMaybeClientDataHash, mCallback::onSignResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.UNKNOWN_ERROR);
        verify(mBrowserBridge, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.FAILURE), any());
    }

    @Test
    @SmallTest
    public void testStartPrefetchRequest_default_success() {
        mRequestOptions.isConditional = true;

        int result = mCredManHelper.startPrefetchRequest(mContext, mFrameHost, mRequestOptions,
                mOriginString,
                /*isCrossOrigin=*/false,
                /*maybeClientDataHash=*/null, mCallback::onSignResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin()).isEqualTo(mOriginString);
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option).isNotNull();
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(option.getCredentialRetrievalData().getString(
                           "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_get_request}");
        assertThat(option.isSystemProviderRequired()).isFalse();
        assertThat(mCallback.getStatus()).isNull();
        verify(mBrowserBridge, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), any());
        verify(mBrowserBridge, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(eq(CredManPrepareRequestEnum.SENT_REQUEST));
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(
                        eq(CredManPrepareRequestEnum.SUCCESS_HAS_RESULTS));
    }

    @Test
    @SmallTest
    public void testStartPrefetchRequest_unknownError_unknownError() {
        mRequestOptions.isConditional = true;
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_UNKNOWN", "Message"));

        int result = mCredManHelper.startPrefetchRequest(mContext, mFrameHost, mRequestOptions,
                mOriginString,
                /*isCrossOrigin=*/false,
                /*maybeClientDataHash=*/null, mCallback::onSignResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.UNKNOWN_ERROR);
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(eq(CredManPrepareRequestEnum.SENT_REQUEST));
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(eq(CredManPrepareRequestEnum.FAILURE));
        verify(mMetricsHelper, times(0)).recordCredmanPrepareRequestDuration(anyLong());
    }

    @Test
    @SmallTest
    public void testCancelConditionalGetAssertion_whileWaitingForSelection_notAllowedError() {
        mRequestOptions.isConditional = true;

        mCredManHelper.startPrefetchRequest(mContext, mFrameHost, mRequestOptions, mOriginString,
                /*isCrossOrigin=*/false,
                /*maybeClientDataHash=*/null, mCallback::onSignResponse, mErrorCallback);
        mCredManHelper.cancelConditionalGetAssertion(mFrameHost);

        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.ABORT_ERROR);
        verify(mBrowserBridge, times(1)).cleanupCredManRequest(any());
        verify(mBrowserBridge, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, never()).reportGetCredentialMetrics(anyInt(), any());
    }

    @Test
    @SmallTest
    public void
    testStartGetRequestAfterStartPrefetchRequest_userCancelWhileWaitingForSelection_doesNotCancelConditionalRequest() {
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        mRequestOptions.isConditional = true;

        int result = mCredManHelper.startPrefetchRequest(mContext, mFrameHost, mRequestOptions,
                mOriginString,
                /*isCrossOrigin=*/false,
                /*maybeClientDataHash=*/null, mCallback::onSignResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mMetricsHelper, times(1)).recordCredmanPrepareRequestDuration(anyLong());

        // Setup the test for startGetRequest:
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_USER_CANCELED", "Message"));
        verify(mBrowserBridge, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), callbackCaptor.capture());

        // Trigger the startPrefetchRequest's startGetRequest:
        callbackCaptor.getValue().onResult(true);

        assertThat(mCallback.getStatus()).isNull();
        verify(mBrowserBridge, never()).cleanupRequest(any());
        verify(mBrowserBridge, never()).cleanupCredManRequest(any());
        verify(mBrowserBridge, times(1)).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.CANCELLED), any());
    }

    @Test
    @SmallTest
    public void
    testStartGetRequestAfterStartPrefetchRequest_userSelectsPassword_canHavePasswordResponse() {
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        mRequestOptions.isConditional = true;

        int result = mCredManHelper.startPrefetchRequest(mContext, mFrameHost, mRequestOptions,
                mOriginString,
                /*isCrossOrigin=*/false,
                /*maybeClientDataHash=*/null, mCallback::onSignResponse, mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);
        verify(mMetricsHelper, times(1)).recordCredmanPrepareRequestDuration(anyLong());
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);

        // Setup the test for startGetRequest:
        String username = "coolUserName";
        String password = "38kay5er1sp0r38";
        mCredentialManager.setCredManGetResponseCredential(
                new FakeAndroidPasswordCredential(username, password));
        verify(mBrowserBridge, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), callbackCaptor.capture());

        // Trigger the startPrefetchRequest's startGetRequest:
        callbackCaptor.getValue().onResult(true);

        credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(2);
        List<FakeAndroidCredentialOption> credentialOptions = credManRequest.getCredentialOptions();
        assertThat(credentialOptions.get(0).getType())
                .isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(credentialOptions.get(1).getType())
                .isEqualTo("android.credentials.TYPE_PASSWORD_CREDENTIAL");
        assertThat(credentialOptions.get(1).getCandidateQueryData().containsKey(
                           "com.android.chrome.PASSWORDS_ONLY_FOR_THE_CHANNEL"))
                .isTrue();

        verify(mBrowserBridge, never()).onCredManUiClosed(any(), anyBoolean());
        // A password is selected, the callback will not be signed.
        assertThat(mCallback.getStatus()).isNull();

        verify(mBrowserBridge, times(1))
                .onPasswordCredentialReceived(any(), eq(username), eq(password));
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SUCCESS_PASSWORD), any());
    }
}
