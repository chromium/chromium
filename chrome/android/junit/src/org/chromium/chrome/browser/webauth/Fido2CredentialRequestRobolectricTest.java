// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

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

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.webauthn.CredManHelper;
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
    WebAuthnBrowserBridge mBrowserBridgeMock;
    @Mock
    CredManHelper mCredManHelperMock;

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

        // Reset any cached evaluation of whether CredMan should be supported.
        Fido2CredentialRequest.sCredManSupport = 0;

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

        mRequest.setOverrideVersionCheckForTesting(true);
        mRequest.overrideBrowserBridgeForTesting(mBrowserBridgeMock);
        mRequest.setCredManHelperForTesting(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabled() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        verify(mCredManHelperMock, times(1))
                .startMakeRequest(any(), any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManDisabled_notUsed() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);

        final byte[] clientDataHash = new byte[] {1, 2, 3};
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, /*frameHost=*/null,
                clientDataHash, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        verify(mCredManHelperMock, times(0))
                .startMakeRequest(any(), any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManDisabled_stillUsedForHybrid() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        testValues.addFeatureFlagOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN_FOR_HYBRID, true);
        FeatureList.setTestValues(testValues);

        final byte[] clientDataHash = new byte[] {1, 2, 3};
        mRequest.setIsHybridRequest(true);
        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions, /*frameHost=*/null,
                clientDataHash, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        verify(mCredManHelperMock, times(1))
                .startMakeRequest(any(), any(), any(), any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testMakeCredential_rkDisabledWithExplicitHash_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;
        final byte[] clientDataHash = new byte[] {1, 2, 3};

        mRequest.handleMakeCredentialRequest(mActivity, mCreationOptions,
                /*frameHost=*/null, clientDataHash, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        verify(mCredManHelperMock, times(0))
                .startMakeRequest(any(), any(), any(), any(), any(), any(), any());
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
        verify(mCredManHelperMock, times(0))
                .startMakeRequest(any(), any(), any(), any(), any(), any(), any());
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
        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabled_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                mCallback::onSignResponse, errorStatus -> mCallback.onError(errorStatus));

        String originString = Fido2CredentialRequest.convertOriginToString(mOrigin);
        verify(mCredManHelperMock)
                .startGetRequest(eq(mActivity), eq(mFrameHost), eq(mRequestOptions),
                        eq(originString),
                        /*isCrossOrigin=*/eq(false), /*maybeClientDataHash=*/eq(null),
                        /*getCallback=*/any(),
                        /*errorCallback=*/any());
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManDisabled_notUsed() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        FeatureList.setTestValues(testValues);

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        verifyNoInteractions(mCredManHelperMock);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManDisabled_stillUsedForHybrid() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN, false);
        testValues.addFeatureFlagOverride(
                DeviceFeatureList.WEBAUTHN_ANDROID_CRED_MAN_FOR_HYBRID, true);
        FeatureList.setTestValues(testValues);

        mRequest.setIsHybridRequest(true);
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        verify(mCredManHelperMock)
                .startGetRequest(eq(mActivity), eq(mFrameHost), eq(mRequestOptions),
                        /*originString=*/any(),
                        /*isCrossOrigin=*/eq(false), /*maybeClientDataHash=*/eq(null),
                        /*getCallback=*/any(),
                        /*errorCallback=*/any());
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

        verifyNoInteractions(mCredManHelperMock);
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

        verifyNoInteractions(mCredManHelperMock);
        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mFido2ApiCallHelper.mClientDataHash).isEqualTo(clientDataHash);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManNoCredentials_fallbackToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mFido2ApiCallHelper.mCredentialsError = new IllegalStateException("injected error");
        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        ArgumentCaptor<Runnable> setNoCredentialsParamCaptor =
                ArgumentCaptor.forClass(Runnable.class);
        verify(mCredManHelperMock).setNoCredentialsFallback(setNoCredentialsParamCaptor.capture());
        verify(mCredManHelperMock)
                .startGetRequest(any(), any(), any(), any(), anyBoolean(), any(), any(), any());

        // Now run the no credentials fallback action:
        setNoCredentialsParamCaptor.getValue().run();

        assertThat(mFido2ApiCallHelper.mGetAssertionCalled).isTrue();
        assertThat(mCallback.getStatus())
                .isEqualTo(Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
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

        verify(mCredManHelperMock)
                .startGetRequest(any(), any(), any(), any(), anyBoolean(), any(), any(), any());
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

        verify(mCredManHelperMock)
                .startGetRequest(any(), any(), any(), any(), anyBoolean(), any(), any(), any());
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

        verifyNoInteractions(mCredManHelperMock);
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
                mCallback::onSignResponse, errorStatus -> mCallback.onError(errorStatus));

        String originString = Fido2CredentialRequest.convertOriginToString(mOrigin);
        verify(mCredManHelperMock, times(1))
                .startPrefetchRequest(eq(mActivity), eq(mFrameHost), eq(mRequestOptions),
                        eq(originString),
                        /*isCrossOrigin=*/eq(false), /*maybeClientDataHash=*/eq(null),
                        /*getCallback=*/any(),
                        /*errorCallback=*/any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledRpCancelWhileIdle_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mActivity, mRequestOptions, mFrameHost,
                /*maybeClientDataHash=*/null, mOrigin, mOrigin, /*payment=*/null,
                mCallback::onSignResponse, errorStatus -> mCallback.onError(errorStatus));

        mRequest.cancelConditionalGetAssertion(mFrameHost);

        // CredManHelper class is responsible to return the status.
        assertThat(mCallback.getStatus()).isEqualTo(null);
        verify(mCredManHelperMock, times(1)).cancelConditionalGetAssertion(eq(mFrameHost));
        verify(mBrowserBridgeMock, never()).cleanupRequest(any());
        verify(mBrowserBridgeMock, never()).onCredManUiClosed(any(), anyBoolean());
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
