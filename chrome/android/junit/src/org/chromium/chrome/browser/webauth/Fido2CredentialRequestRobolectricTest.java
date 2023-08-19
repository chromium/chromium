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

import android.app.Activity;
import android.app.PendingIntent;
import android.net.Uri;
import android.os.Build;

import androidx.test.filters.SmallTest;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

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
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.webauthn.CredManMetricsHelper;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManCreateRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManGetRequestEnum;
import org.chromium.components.webauthn.CredManMetricsHelper.CredManPrepareRequestEnum;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.WebAuthnBrowserBridge;
import org.chromium.components.webauthn.WebAuthnCredentialDetails;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.device.DeviceFeatureList;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class Fido2CredentialRequestRobolectricTest {
    private FakeAndroidCredentialManager mCredentialManager;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private Origin mOrigin;
    private FakeFido2ApiCallHelper mFido2ApiCallHelper;

    @Mock
    private RenderFrameHost mFrameHost;
    @Mock
    GURLUtils.Natives mGURLUtilsJniMock;
    @Mock
    ClientDataJsonImpl.Natives mClientDataJsonImplMock;
    @Mock
    Activity mActivity;
    @Mock
    CredManMetricsHelper mMetricsHelper;
    @Mock
    WebAuthnBrowserBridge mBrowserBridgeMock;

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Before
    public void setUp() throws Exception {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, true);
        FeatureList.setTestValues(testValues);

        MockitoAnnotations.initMocks(this);

        org.chromium.url.internal.mojom.Origin mojomOrigin =
                new org.chromium.url.internal.mojom.Origin();
        mojomOrigin.scheme = "https";
        mojomOrigin.host = "subdomain.example.test";
        mojomOrigin.port = 443;
        GURL gurl = new GURL(
                "https://subdomain.example.test:443/content/test/data/android/authenticator.html");
        mOrigin = new Origin(mojomOrigin);

        mMocker.mock(GURLUtilsJni.TEST_HOOKS, mGURLUtilsJniMock);
        Mockito.when(mGURLUtilsJniMock.getOrigin(any(String.class)))
                .thenReturn("https://subdomain.example.test:443");

        mFido2ApiCallHelper = new FakeFido2ApiCallHelper();
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        // Set rk=required and empty allowlist on the assumption that most test cases care about
        // exercising the passkeys case.
        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.REQUIRED;
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[0];

        mRequest = new Fido2CredentialRequest(
                /*intentSender=*/null);

        Fido2ApiTestHelper.mockFido2CredentialRequestJni(mMocker);
        Fido2ApiTestHelper.mockClientDataJson(mMocker, "{}");

        mCallback = Fido2ApiTestHelper.getAuthenticatorCallback();

        Mockito.when(mFrameHost.getLastCommittedURL()).thenReturn(gurl);
        Mockito.when(mFrameHost.getLastCommittedOrigin()).thenReturn(mOrigin);
        Mockito.when(mFrameHost.performMakeCredentialWebAuthSecurityChecks(
                             any(String.class), any(Origin.class), anyBoolean()))
                .thenReturn(0);
        Mockito.when(mFrameHost.performGetAssertionWebAuthSecurityChecks(
                             any(String.class), any(Origin.class), anyBoolean()))
                .thenReturn(new WebAuthSecurityChecksResults(AuthenticatorStatus.SUCCESS, false));

        mCredentialManager = new FakeAndroidCredentialManager();
        mRequest.setOverrideVersionCheckForTesting(true);
        mRequest.setCredManClassesForTesting(mCredentialManager,
                FakeAndroidCredManCreateRequest.Builder.class,
                FakeAndroidCredManGetRequest.Builder.class,
                FakeAndroidCredentialOption.Builder.class, mMetricsHelper);

        mRequest.overrideBrowserBridgeForTesting(mBrowserBridgeMock);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabled_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManCreateRequest credManRequest = mCredentialManager.getCreateRequest();
        assertThat(credManRequest).isNotNull();
        ;
        assertThat(credManRequest.getOrigin())
                .isEqualTo(Fido2CredentialRequest.convertOriginToString(mOrigin));
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
    public void testMakeCredential_credManEnabledWithExplicitHash_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        final byte[] clientDataHash = new byte[] {1, 2, 3};
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, /*frameHost=*/null,
                clientDataHash, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManCreateRequest credManRequest = mCredentialManager.getCreateRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin())
                .isEqualTo(Fido2CredentialRequest.convertOriginToString(mOrigin));
        assertThat(credManRequest.getCredentialData().getByteArray(
                           "androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(clientDataHash);
        assertThat(mCallback.getStatus()).isEqualTo(Integer.valueOf(AuthenticatorStatus.SUCCESS));
    }

    @Test
    @SmallTest
    public void testMakeCredential_rkDisabledWithExplicitHash_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;
        final byte[] clientDataHash = new byte[] {1, 2, 3};
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, /*frameHost=*/null,
                clientDataHash, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManCreateRequest credManRequest = mCredentialManager.getCreateRequest();
        assertThat(credManRequest).isNull();
        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
        assertThat(mFido2ApiCallHelper.mClientDataHash).isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testMakeCredential_rkDiscouraged_goesToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
    }

    @Test
    @SmallTest
    public void testMakeCredential_paymentsEnabled_goesToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCreationOptions.isPaymentCredentialCreation = true;
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mFido2ApiCallHelper.mMakeCredentialCalled).isTrue();
        verify(mMetricsHelper, times(0)).recordCredManCreateRequestHistogram(anyInt());
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabledUserCancel_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_USER_CANCELED", "Message"));
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.CANCELLED);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabledInvalidStateError_credentialExcluded() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                Fido2CredentialRequest
                        .CRED_MAN_EXCEPTION_CREATE_CREDENTIAL_TYPE_INVALID_STATE_ERROR,
                "Message"));
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED));
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.SUCCESS);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_UNKNOWN", "Message"));
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        verify(mMetricsHelper, times(1))
                .recordCredManCreateRequestHistogram(CredManCreateRequestEnum.FAILURE);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabled_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin())
                .isEqualTo(Fido2CredentialRequest.convertOriginToString(mOrigin));
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
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SENT_REQUEST), any());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SUCCESS_PASSKEY), any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledWithExplicitHash_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, /*frameHost=*/null,
                clientDataHash, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option.getCredentialRetrievalData().getByteArray(
                           "androidx.credentials.BUNDLE_KEY_CLIENT_DATA_HASH"))
                .isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListMatchWithExplicitHash_goesToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        WebAuthnCredentialDetails details = new WebAuthnCredentialDetails();
        details.mCredentialId = descriptor.id;
        mFido2ApiCallHelper.mCredentials = new ArrayList<>();
        mFido2ApiCallHelper.mCredentials.add(details);

        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, /*frameHost=*/null,
                clientDataHash, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNull();
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mFido2ApiCallHelper.mClientDataHash).isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testGetAssertion_prfInputsHashed_goesToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        final byte[] clientDataHash = new byte[] {1, 2, 3, 4};
        mRequestOptions.extensions.prfInputsHashed = true;
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, /*frameHost=*/null,
                clientDataHash, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNull();
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mFido2ApiCallHelper.mClientDataHash).isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManNoCredentials_fallbackToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_NO_CREDENTIAL", "Message"));
        mFido2ApiCallHelper.mCredentialsError = new IllegalStateException("injected error");
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(
                credManRequest.getData().getBoolean(
                        "androidx.credentials.BUNDLE_KEY_PREFER_IMMEDIATELY_AVAILABLE_CREDENTIALS"))
                .isTrue();
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledUserCancel_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_USER_CANCELED", "Message"));
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.CANCELLED), any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_UNKNOWN", "Message"));
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.FAILURE), any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListNoMatch_goesToCredMan() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListEnumerationFails_goesToCredMan() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        mFido2ApiCallHelper.mCredentialsError = new IllegalStateException("injected error");

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isFalse();
    }

    @Test
    @SmallTest
    public void testGetAssertion_allowListMatch_goesToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {1, 2, 3, 4};
        descriptor.transports = new int[] {0};
        mRequestOptions.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        WebAuthnCredentialDetails details = new WebAuthnCredentialDetails();
        details.mCredentialId = descriptor.id;
        mFido2ApiCallHelper.mCredentials = new ArrayList<>();
        mFido2ApiCallHelper.mCredentials.add(details);

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNull();
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledSuccess_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getOrigin())
                .isEqualTo(Fido2CredentialRequest.convertOriginToString(mOrigin));
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        assertThat(option).isNotNull();
        assertThat(option.getType()).isEqualTo("androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        assertThat(option.getCredentialRetrievalData().getString(
                           "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"))
                .isEqualTo("{serialized_get_request}");
        assertThat(option.isSystemProviderRequired()).isFalse();
        assertThat(mCallback.getStatus()).isNull();
        verify(mBrowserBridgeMock, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(eq(CredManPrepareRequestEnum.SENT_REQUEST));
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(
                        eq(CredManPrepareRequestEnum.SUCCESS_HAS_RESULTS));
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_UNKNOWN", "Message"));

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(eq(CredManPrepareRequestEnum.SENT_REQUEST));
        verify(mMetricsHelper, times(1))
                .recordCredmanPrepareRequestHistogram(eq(CredManPrepareRequestEnum.FAILURE));
        verify(mMetricsHelper, times(0)).recordCredmanPrepareRequestDuration(anyLong());
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledRpCancelWhileIdle_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        mRequest.cancelConditionalGetAssertion(mFrameHost);
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.ABORT_ERROR));
        verify(mBrowserBridgeMock, times(1)).cleanupRequest(any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, never()).reportGetCredentialMetrics(anyInt(), any());
    }

    @Test
    @SmallTest
    public void
    testConditionalGetAssertion_credManEnabledUserCancelWhileIdle_DoesNotCancelConditionalRequest() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        verify(mMetricsHelper, times(1)).recordCredmanPrepareRequestDuration(anyLong());

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_USER_CANCELED", "Message"));

        verify(mBrowserBridgeMock, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), callbackCaptor.capture());

        callbackCaptor.getValue().onResult(true);

        assertThat(mCallback.getStatus()).isNull();
        verify(mBrowserBridgeMock, never()).cleanupRequest(any());
        verify(mBrowserBridgeMock, times(1)).onCredManUiClosed(any(), anyBoolean());
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.CANCELLED), any());
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledWithPasswords_canHavePasswordResponse() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        verify(mMetricsHelper, times(1)).recordCredmanPrepareRequestDuration(anyLong());

        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        assertThat(credManRequest).isNotNull();
        assertThat(credManRequest.getCredentialOptions()).hasSize(1);

        String username = "coolUserName";
        String password = "38kay5er1sp0r38";
        mCredentialManager.setCredManGetResponseCredential(
                new FakeAndroidPasswordCredential(username, password));

        verify(mBrowserBridgeMock, times(1))
                .onCredManConditionalRequestPending(any(), anyBoolean(), callbackCaptor.capture());

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

        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
        // A password is selected, the callback will not be signed.
        assertThat(mCallback.getStatus()).isNull();

        verify(mBrowserBridgeMock, times(1))
                .onPasswordCredentialReceived(any(), eq(username), eq(password));
        verify(mMetricsHelper, times(1))
                .reportGetCredentialMetrics(eq(CredManGetRequestEnum.SUCCESS_PASSWORD), any());
    }

    static class FakeFido2ApiCallHelper extends Fido2ApiCallHelper {
        public boolean mMakeCredentialCalled;
        public boolean mGetAssertionCalled;
        public List<WebAuthnCredentialDetails> mCredentials;
        public Exception mCredentialsError;
        public byte[] mClientDataHash;

        @Override
        public boolean arePlayServicesAvailable() {
            return true;
        }

        @Override
        public void invokeFido2GetCredentials(String relyingPartyId,
                OnSuccessListener<List<WebAuthnCredentialDetails>> successCallback,
                OnFailureListener failureCallback) {
            if (mCredentialsError != null) {
                failureCallback.onFailure(mCredentialsError);
                return;
            }

            List<WebAuthnCredentialDetails> credentials;
            if (mCredentials == null) {
                credentials = new ArrayList();
            } else {
                credentials = mCredentials;
                mCredentials = null;
            }

            successCallback.onSuccess(credentials);
        }

        @Override
        public void invokeFido2MakeCredential(PublicKeyCredentialCreationOptions options, Uri uri,
                byte[] clientDataHash, OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback) throws NoSuchAlgorithmException {
            mMakeCredentialCalled = true;
            mClientDataHash = clientDataHash;

            if (mCredentialsError != null) {
                failureCallback.onFailure(mCredentialsError);
                return;
            }
            // Don't make any actual calls to Play Services.
        }

        @Override
        public void invokeFido2GetAssertion(PublicKeyCredentialRequestOptions options, Uri uri,
                byte[] clientDataHash, OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback) {
            mGetAssertionCalled = true;
            mClientDataHash = clientDataHash;

            if (mCredentialsError != null) {
                failureCallback.onFailure(mCredentialsError);
                return;
            }
            // Don't make any actual calls to Play Services.
        }
    }
}
