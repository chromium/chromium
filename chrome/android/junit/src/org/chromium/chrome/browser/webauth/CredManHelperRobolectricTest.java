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
import static org.mockito.Mockito.when;

import android.content.Context;
import android.credentials.CreateCredentialException;
import android.credentials.CreateCredentialRequest;
import android.credentials.CreateCredentialResponse;
import android.credentials.Credential;
import android.credentials.CredentialManager;
import android.credentials.CredentialOption;
import android.credentials.GetCredentialException;
import android.credentials.GetCredentialRequest;
import android.credentials.GetCredentialResponse;
import android.credentials.PrepareGetCredentialResponse;
import android.os.Build;
import android.os.Bundle;

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
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.webauthn.Barrier;
import org.chromium.components.webauthn.CredManHelper;
import org.chromium.components.webauthn.CredManMetricsHelper;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManCreateRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManGetRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManPrepareRequestEnum;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.WebAuthnBrowserBridge;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            ShadowCreateCredentialRequest.class,
            ShadowCreateCredentialRequest.ShadowBuilder.class,
            ShadowCreateCredentialResponse.class,
            ShadowCreateCredentialException.class,
            ShadowCredential.class,
            ShadowCredentialManager.class,
            ShadowCredentialOption.class,
            ShadowCredentialOption.ShadowBuilder.class,
            ShadowGetCredentialException.class,
            ShadowGetCredentialRequest.class,
            ShadowGetCredentialRequest.ShadowBuilder.class,
            ShadowGetCredentialResponse.class,
            ShadowPrepareGetCredentialResponse.class,
            ShadowWebContentStatics.class
        })
public class CredManHelperRobolectricTest {
    private CredManHelper mCredManHelper;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private String mOriginString = "https://subdomain.coolwebsitekayserispor.com";
    private byte[] mMaybeClientDataHash = new byte[] {1, 2, 3};

    private CredentialManager mCredentialManager = Shadow.newInstanceOf(CredentialManager.class);
    @Mock private Context mContext;
    @Mock private RenderFrameHost mFrameHost;
    @Mock private WebContents mWebContents;
    @Mock private CredManMetricsHelper mMetricsHelper;
    @Mock private WebAuthnBrowserBridge mBrowserBridge;
    @Mock private Callback<Integer> mErrorCallback;
    @Mock private Barrier mBarrier;

    private CredManHelper.BridgeProvider mBridgeProvider =
            new CredManHelper.BridgeProvider() {
                @Override
                public WebAuthnBrowserBridge getBridge() {
                    return mBrowserBridge;
                }
            };

    @Rule public JniMocker mMocker = new JniMocker();

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

        mCredManHelper = new CredManHelper(mBridgeProvider, /* playServicesAvailable= */ true);
        mCredManHelper.setMetricsHelperForTesting(mMetricsHelper);
        when(mContext.getSystemService(Context.CREDENTIAL_SERVICE)).thenReturn(mCredentialManager);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_default_success() {
        int result =
                mCredManHelper.startMakeRequest(
                        mContext,
                        mFrameHost,
                        mCreationOptions,
                        mOriginString,
                        /* maybeClientDataHash= */ null,
                        mCallback::onRegisterResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        CreateCredentialRequest createCredentialRequest =
                shadowCredentialManager.getCreateCredentialRequest();
        assertThat(createCredentialRequest).isNotNull();
        assertThat(createCredentialRequest.getOrigin()).isEqualTo(mOriginString);
        assertThat(createCredentialRequest.getType())
                .isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(
                        createCredentialRequest
                                .getCredentialData()
                                .getString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_make_request}");
        assertThat(createCredentialRequest.alwaysSendAppInfoToProvider()).isTrue();
        assertThat(
                        createCredentialRequest
                                .getCandidateQueryData()
                                .containsKey("com.android.chrome.CHANNEL"))
                .isTrue();

        shadowCredentialManager
                .getCreateCredentialCallback()
                .onResult(Shadow.newInstanceOf(CreateCredentialResponse.class));
        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));

        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SUCCESS);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_withExplicitHash_success() {
        int result =
                mCredManHelper.startMakeRequest(
                        mContext,
                        mFrameHost,
                        mCreationOptions,
                        mOriginString,
                        mMaybeClientDataHash,
                        mCallback::onRegisterResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        CreateCredentialRequest createCredentialRequest =
                shadowCredentialManager.getCreateCredentialRequest();
        assertThat(createCredentialRequest).isNotNull();
        assertThat(createCredentialRequest.getOrigin()).isEqualTo(mOriginString);
        assertThat(
                        createCredentialRequest
                                .getCredentialData()
                                .getByteArray("androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(mMaybeClientDataHash);

        shadowCredentialManager
                .getCreateCredentialCallback()
                .onResult(Shadow.newInstanceOf(CreateCredentialResponse.class));
        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_userCancel_notAllowedError() {
        int result =
                mCredManHelper.startMakeRequest(
                        mContext,
                        mFrameHost,
                        mCreationOptions,
                        mOriginString,
                        mMaybeClientDataHash,
                        mCallback::onRegisterResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        CreateCredentialException exception = Shadow.newInstanceOf(CreateCredentialException.class);
        ShadowCreateCredentialException shadowException = Shadow.extract(exception);
        shadowException.setType("android.credentials.CreateCredentialException.TYPE_USER_CANCELED");
        shadowCredentialManager.getCreateCredentialCallback().onError(exception);

        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.CANCELLED);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_invalidStateError_credentialExcluded() {
        int result =
                mCredManHelper.startMakeRequest(
                        mContext,
                        mFrameHost,
                        mCreationOptions,
                        mOriginString,
                        mMaybeClientDataHash,
                        mCallback::onRegisterResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        CreateCredentialException exception = Shadow.newInstanceOf(CreateCredentialException.class);
        ShadowCreateCredentialException shadowException = Shadow.extract(exception);
        shadowException.setType(
                CredManHelper.CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR);
        shadowCredentialManager.getCreateCredentialCallback().onError(exception);

        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.CREDENTIAL_EXCLUDED);
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SUCCESS);
    }

    @Test
    @SmallTest
    public void testStartMakeRequest_unknownError_unknownError() {
        int result =
                mCredManHelper.startMakeRequest(
                        mContext,
                        mFrameHost,
                        mCreationOptions,
                        mOriginString,
                        mMaybeClientDataHash,
                        mCallback::onRegisterResponse,
                        mErrorCallback);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        CreateCredentialException exception = Shadow.newInstanceOf(CreateCredentialException.class);
        ShadowCreateCredentialException shadowException = Shadow.extract(exception);
        shadowException.setType("android.credentials.CreateCredentialException.TYPE_UNKNOWN");
        shadowCredentialManager.getCreateCredentialCallback().onError(exception);

        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.UNKNOWN_ERROR);
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.FAILURE);
    }

    @Test
    @SmallTest
    public void testStartGetRequest_default_success() {
        int result =
                mCredManHelper.startGetRequest(
                        mContext,
                        mFrameHost,
                        mRequestOptions,
                        mOriginString,
                        /* isCrossOrigin= */ false,
                        /* maybeClientDataHash= */ null,
                        mCallback::onSignResponse,
                        mErrorCallback,
                        /* ignoreGpm= */ false);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialRequest credManRequest = shadowCredentialManager.getGetCredentialRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(
                        credManRequest
                                .getData()
                                .containsKey(
                                        "androidx.credentials.BUNDLE_KEY_PREFER_UI_BRANDING_COMPONENT_NAME"))
                .isTrue();
        assertThat(credManRequest.getOrigin()).isEqualTo(mOriginString);
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);

        CredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option).isNotNull();
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(
                        option.getCredentialRetrievalData()
                                .getString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_get_request}");
        assertThat(option.getCandidateQueryData().containsKey("com.android.chrome.CHANNEL"))
                .isTrue();
        assertThat(
                        option.getCandidateQueryData()
                                .getBoolean("com.android.chrome.GPM_IGNORE", false))
                .isFalse();
        assertThat(option.isSystemProviderRequired()).isFalse();

        GetCredentialResponse response = new GetCredentialResponse(createPasskeyCredential());
        shadowCredentialManager.getGetCredentialCallback().onResult(response);

        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));
        verify(mBrowserBridge, times(1)).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SENT_REQUEST), any());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SUCCESS_PASSKEY), any());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_withExplicitHash_success() {
        int result =
                mCredManHelper.startGetRequest(
                        mContext,
                        mFrameHost,
                        mRequestOptions,
                        mOriginString,
                        /* isCrossOrigin= */ false,
                        mMaybeClientDataHash,
                        mCallback::onSignResponse,
                        mErrorCallback,
                        /* ignoreGpm= */ false);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialRequest credManRequest = shadowCredentialManager.getGetCredentialRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);
        CredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(
                        option.getCredentialRetrievalData()
                                .getByteArray("androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(mMaybeClientDataHash);
    }

    @Test
    @SmallTest
    public void testStartGetRequest_noCredentials_noCredentialsFallbackCalled() {
        Runnable noCredentialsFallback = Mockito.mock(Runnable.class);
        mCredManHelper.setNoCredentialsFallback(noCredentialsFallback);

        int result =
                mCredManHelper.startGetRequest(
                        mContext,
                        mFrameHost,
                        mRequestOptions,
                        mOriginString,
                        /* isCrossOrigin= */ false,
                        mMaybeClientDataHash,
                        mCallback::onSignResponse,
                        mErrorCallback,
                        /* ignoreGpm= */ false);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialRequest credManRequest = shadowCredentialManager.getGetCredentialRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(
                        credManRequest
                                .getData()
                                .getBoolean(
                                        "androidx.credentials.BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS"))
                .isTrue();

        GetCredentialException exception =
                new GetCredentialException(GetCredentialException.TYPE_NO_CREDENTIAL, "Message");
        shadowCredentialManager.getGetCredentialCallback().onError(exception);
        verify(noCredentialsFallback, times(1)).run();
        verify(mBrowserBridge, times(1)).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_noCredentials_errorHandlerCalledIfNoFallbackSet() {
        Runnable noCredentialsFallback = Mockito.mock(Runnable.class);

        int result =
                mCredManHelper.startGetRequest(
                        mContext,
                        mFrameHost,
                        mRequestOptions,
                        mOriginString,
                        /* isCrossOrigin= */ false,
                        mMaybeClientDataHash,
                        mCallback::onSignResponse,
                        mErrorCallback,
                        /* ignoreGpm= */ false);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialException exception =
                new GetCredentialException(GetCredentialException.TYPE_NO_CREDENTIAL, "Message");
        shadowCredentialManager.getGetCredentialCallback().onError(exception);
        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
    }

    @Test
    @SmallTest
    public void testStartGetRequest_userCancel_notAllowedError() {
        int result =
                mCredManHelper.startGetRequest(
                        mContext,
                        mFrameHost,
                        mRequestOptions,
                        mOriginString,
                        /* isCrossOrigin= */ false,
                        mMaybeClientDataHash,
                        mCallback::onSignResponse,
                        mErrorCallback,
                        /* ignoreGpm= */ false);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialException exception =
                new GetCredentialException(GetCredentialException.TYPE_USER_CANCELED, "Message");
        shadowCredentialManager.getGetCredentialCallback().onError(exception);

        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.NOT_ALLOWED_ERROR);
        verify(mBrowserBridge, times(1)).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.CANCELLED), any());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_unknownError_unknownError() {
        int result =
                mCredManHelper.startGetRequest(
                        mContext,
                        mFrameHost,
                        mRequestOptions,
                        mOriginString,
                        /* isCrossOrigin= */ false,
                        mMaybeClientDataHash,
                        mCallback::onSignResponse,
                        mErrorCallback,
                        /* ignoreGpm= */ false);

        assertThat(result).isEqualTo(AuthenticatorStatus.SUCCESS);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialException exception =
                new GetCredentialException(GetCredentialException.TYPE_UNKNOWN, "Message");
        shadowCredentialManager.getGetCredentialCallback().onError(exception);

        verify(mErrorCallback, times(1)).onResult(AuthenticatorStatus.UNKNOWN_ERROR);
        verify(mBrowserBridge, times(1)).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.FAILURE), any());
    }

    @Test
    @SmallTest
    public void testStartPrefetchRequest_default_success() {
        mRequestOptions.isConditional = true;

        mCredManHelper.startPrefetchRequest(
                mContext,
                mFrameHost,
                mRequestOptions,
                mOriginString,
                /* isCrossOrigin= */ false,
                /* maybeClientDataHash= */ null,
                mCallback::onSignResponse,
                mErrorCallback,
                mBarrier,
                /* ignoreGpm= */ false);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialRequest credManRequest = shadowCredentialManager.getGetCredentialRequest();

        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin()).isEqualTo(mOriginString);
        CredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option).isNotNull();
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(
                        option.getCredentialRetrievalData()
                                .getString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_get_request}");
        assertThat(option.isSystemProviderRequired()).isFalse();

        PrepareGetCredentialResponse prepareGetCredentialResponse =
                Shadow.newInstanceOf(PrepareGetCredentialResponse.class);
        shadowCredentialManager
                .getPrepareGetCredentialCallback()
                .onResult(prepareGetCredentialResponse);

        verify(mBarrier, never()).onCredManFailed(anyInt());
        ArgumentCaptor<Runnable> credManCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrier).onCredManSuccessful(credManCallSuccessfulRunback.capture());

        credManCallSuccessfulRunback.getValue().run();

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

        mCredManHelper.startPrefetchRequest(
                mContext,
                mFrameHost,
                mRequestOptions,
                mOriginString,
                /* isCrossOrigin= */ false,
                /* maybeClientDataHash= */ null,
                mCallback::onSignResponse,
                mErrorCallback,
                mBarrier,
                /* ignoreGpm= */ false);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        shadowCredentialManager
                .getPrepareGetCredentialCallback()
                .onError(
                        new GetCredentialException(GetCredentialException.TYPE_UNKNOWN, "Message"));

        verify(mBarrier, never()).onCredManFailed(eq(0));
        verify(mBarrier, times(1)).onCredManFailed(eq(AuthenticatorStatus.UNKNOWN_ERROR));
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

        mCredManHelper.startPrefetchRequest(
                mContext,
                mFrameHost,
                mRequestOptions,
                mOriginString,
                /* isCrossOrigin= */ false,
                /* maybeClientDataHash= */ null,
                mCallback::onSignResponse,
                mErrorCallback,
                mBarrier,
                /* ignoreGpm= */ false);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        PrepareGetCredentialResponse prepareGetCredentialResponse =
                Shadow.newInstanceOf(PrepareGetCredentialResponse.class);
        shadowCredentialManager
                .getPrepareGetCredentialCallback()
                .onResult(prepareGetCredentialResponse);

        ArgumentCaptor<Runnable> credManCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrier).onCredManSuccessful(credManCallSuccessfulRunback.capture());
        credManCallSuccessfulRunback.getValue().run();

        mCredManHelper.cancelConditionalGetAssertion(mFrameHost);

        verify(mBarrier, times(1)).onCredManCancelled();
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

        mCredManHelper.startPrefetchRequest(
                mContext,
                mFrameHost,
                mRequestOptions,
                mOriginString,
                /* isCrossOrigin= */ false,
                /* maybeClientDataHash= */ null,
                mCallback::onSignResponse,
                mErrorCallback,
                mBarrier,
                /* ignoreGpm= */ false);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        PrepareGetCredentialResponse prepareGetCredentialResponse =
                Shadow.newInstanceOf(PrepareGetCredentialResponse.class);
        shadowCredentialManager
                .getPrepareGetCredentialCallback()
                .onResult(prepareGetCredentialResponse);

        verify(mBarrier, never()).onCredManFailed(anyInt());
        ArgumentCaptor<Runnable> credManCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrier).onCredManSuccessful(credManCallSuccessfulRunback.capture());
        credManCallSuccessfulRunback.getValue().run();

        verify(mMetricsHelper, times(1)).recordCredmanPrepareRequestDuration(anyLong());

        // Setup the test for startGetRequest:
        verify(mBrowserBridge, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), callbackCaptor.capture());

        // Trigger the startPrefetchRequest's startGetRequest:
        callbackCaptor.getValue().onResult(true);
        shadowCredentialManager
                .getGetCredentialCallback()
                .onError(
                        new GetCredentialException(
                                GetCredentialException.TYPE_USER_CANCELED, "Message"));

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

        mCredManHelper.startPrefetchRequest(
                mContext,
                mFrameHost,
                mRequestOptions,
                mOriginString,
                /* isCrossOrigin= */ false,
                /* maybeClientDataHash= */ null,
                mCallback::onSignResponse,
                mErrorCallback,
                mBarrier,
                /* ignoreGpm= */ false);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        PrepareGetCredentialResponse prepareGetCredentialResponse =
                Shadow.newInstanceOf(PrepareGetCredentialResponse.class);
        shadowCredentialManager
                .getPrepareGetCredentialCallback()
                .onResult(prepareGetCredentialResponse);

        verify(mBarrier, never()).onCredManFailed(anyInt());
        ArgumentCaptor<Runnable> credManCallSuccessfulRunback =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mBarrier).onCredManSuccessful(credManCallSuccessfulRunback.capture());
        credManCallSuccessfulRunback.getValue().run();

        verify(mMetricsHelper, times(1)).recordCredmanPrepareRequestDuration(anyLong());

        GetCredentialRequest credManRequest = shadowCredentialManager.getGetCredentialRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);

        verify(mBrowserBridge, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), callbackCaptor.capture());

        String username = "coolUserName";
        String password = "38kay5er1sp0r38";
        GetCredentialResponse response =
                new GetCredentialResponse(createPasswordCredential(username, password));

        // Trigger the startPrefetchRequest's startGetRequest:
        callbackCaptor.getValue().onResult(true);
        shadowCredentialManager.getGetCredentialCallback().onResult(response);

        credManRequest = shadowCredentialManager.getGetCredentialRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(2);
        List<CredentialOption> credentialOptions = credManRequest.getCredentialOptions();
        assertThat(credentialOptions.get(0).getType())
                .isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(credentialOptions.get(1).getType())
                .isEqualTo(Credential.TYPE_PASSWORD_CREDENTIAL);
        assertThat(
                        credentialOptions
                                .get(1)
                                .getCandidateQueryData()
                                .containsKey("com.android.chrome.PASSWORDS_ONLY_FOR_THE_CHANNEL"))
                .isTrue();

        verify(mBrowserBridge, never()).onCredManUiClosed(any(), anyBoolean());
        // A password is selected, the callback will not be signed.
        assertThat(mCallback.getStatus()).isNull();

        verify(mBrowserBridge, times(1))
                .onPasswordCredentialReceived(any(), eq(username), eq(password));
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SUCCESS_PASSWORD), any());
    }

    @Test
    @SmallTest
    public void testStartGetRequest_ignoreGpm_DisablesBrandingAndHasBooleanInBundle() {
        mCredManHelper.startGetRequest(
                mContext,
                mFrameHost,
                mRequestOptions,
                mOriginString,
                /* isCrossOrigin= */ false,
                /* maybeClientDataHash= */ null,
                mCallback::onSignResponse,
                mErrorCallback,
                /* ignoreGpm= */ true);

        ShadowCredentialManager shadowCredentialManager = Shadow.extract(mCredentialManager);
        GetCredentialRequest credManRequest = shadowCredentialManager.getGetCredentialRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(
                        credManRequest
                                .getData()
                                .containsKey(
                                        "androidx.credentials.BUNDLE_KEY_PREFER_UI_BRANDING_COMPONENT_NAME"))
                .isFalse();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);
        CredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option).isNotNull();
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(option.getCandidateQueryData().containsKey("com.android.chrome.GPM_IGNORE"))
                .isTrue();
        assertThat(option.getCandidateQueryData().getBoolean("com.android.chrome.GPM_IGNORE"))
                .isTrue();
    }

    private Credential createPasskeyCredential() {
        Bundle data = new Bundle();
        data.putString("androidx.credentials.BUNDLE_KEY_AUTHENTICATION_RESPONSE_JSON", "json");
        return new Credential("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL", data);
    }

    private Credential createPasswordCredential(String username, String password) {
        Bundle data = new Bundle();
        data.putString("androidx.credentials.BUNDLE_KEY_ID", username);
        data.putString("androidx.credentials.BUNDLE_KEY_PASSWORD", password);
        return new Credential(Credential.TYPE_PASSWORD_CREDENTIAL, data);
    }
}
