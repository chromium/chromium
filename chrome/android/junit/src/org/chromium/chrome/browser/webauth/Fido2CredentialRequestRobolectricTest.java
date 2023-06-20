// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;

import android.app.Activity;
import android.app.PendingIntent;
import android.net.Uri;
import android.os.Build;

import androidx.test.filters.SmallTest;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.WebAuthnBrowserBridge;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.DeviceFeatureList;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
public class Fido2CredentialRequestRobolectricTest {
    private static class MockBrowserBridge extends WebAuthnBrowserBridge {
        private int mOnCredManConditionalRequestPendingCallCount;
        private int mCleanupRequestCallCount;
        private int mOnCredManClosedCallCount;
        private HashMap<String, String> mOnPasswordCredentialReceivedCall;
        private Callback<Boolean> mCredManGetAssertionCallback;

        MockBrowserBridge() {
            mOnCredManConditionalRequestPendingCallCount = 0;
            mCleanupRequestCallCount = 0;
            mOnCredManClosedCallCount = 0;
            mCredManGetAssertionCallback = null;
            mOnPasswordCredentialReceivedCall = new HashMap<>();
        }

        @Override
        public void onCredManConditionalRequestPending(
                RenderFrameHost frameHost, boolean hasResults, Callback<Boolean> fullAssertion) {
            mOnCredManConditionalRequestPendingCallCount += 1;
            mCredManGetAssertionCallback = fullAssertion;
        }

        @Override
        public void cleanupRequest(RenderFrameHost frameHost) {
            mCleanupRequestCallCount += 1;
        }

        @Override
        public void onCredManUiClosed(RenderFrameHost frameHost, boolean success) {
            mOnCredManClosedCallCount += 1;
        }

        @Override
        public void onPasswordCredentialReceived(
                RenderFrameHost frameHost, String username, String password) {
            mOnPasswordCredentialReceivedCall.put("username", username);
            mOnPasswordCredentialReceivedCall.put("password", password);
        }

        int getOnCredManConditionalRequestPendingCallCount() {
            return mOnCredManConditionalRequestPendingCallCount;
        }

        int getCleanupRequestCallCount() {
            return mCleanupRequestCallCount;
        }

        Callback<Boolean> getCredManGetAssertionCallback() {
            return mCredManGetAssertionCallback;
        }

        int getOnCredManClosedCallCount() {
            return mOnCredManClosedCallCount;
        }

        Map<String, String> getOnPasswordCredentialReceivedCall() {
            return mOnPasswordCredentialReceivedCall;
        }
    }

    private FakeAndroidCredentialManager mCredentialManager;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private Origin mOrigin;
    private MockBrowserBridge mMockBrowserBridge;
    private FakeFido2ApiCallHelper mFido2ApiCallHelper;

    @Mock
    private RenderFrameHost mFrameHost;
    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;
    @Mock
    GURLUtils.Natives mGURLUtilsJniMock;
    @Mock
    ClientDataJsonImpl.Natives mClientDataJsonImplMock;
    @Mock
    Activity mActivity;

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

        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        // Set rk=required on the assumption that most test cases care about exercising the passkeys
        // case.
        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.REQUIRED;
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();

        mRequest = new Fido2CredentialRequest(
                /*intentSender=*/null);
        AuthenticatorImpl.overrideFido2CredentialRequestForTesting(mRequest);

        Fido2ApiTestHelper.mockFido2CredentialRequestJni(mMocker);
        Fido2ApiTestHelper.mockClientDataJson(mMocker, "{}");

        mCallback = Fido2ApiTestHelper.getAuthenticatorCallback();

        mRequest.setWebContentsForTesting(mWebContents);

        Mockito.when(mFrameHost.getLastCommittedURL()).thenReturn(gurl);
        Mockito.when(mFrameHost.getLastCommittedOrigin()).thenReturn(mOrigin);
        Mockito.when(mFrameHost.performMakeCredentialWebAuthSecurityChecks(
                             any(String.class), any(Origin.class), anyBoolean()))
                .thenReturn(0);
        Mockito.when(mFrameHost.performGetAssertionWebAuthSecurityChecks(
                             any(String.class), any(Origin.class), anyBoolean()))
                .thenReturn(new WebAuthSecurityChecksResults(AuthenticatorStatus.SUCCESS, false));
        Mockito.when(mWebContents.getTopLevelNativeWindow().getActivity().get())
                .thenReturn(mActivity);

        mCredentialManager = new FakeAndroidCredentialManager();
        mRequest.setOverrideVersionCheckForTesting(true);
        mRequest.setCredManClassesForTesting(mCredentialManager,
                FakeAndroidCredManCreateRequest.Builder.class,
                FakeAndroidCredManGetRequest.Builder.class,
                FakeAndroidCredentialOption.Builder.class);

        mMockBrowserBridge = new MockBrowserBridge();
        mRequest.overrideBrowserBridgeForTesting(mMockBrowserBridge);

        mFido2ApiCallHelper = new FakeFido2ApiCallHelper();
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabled_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManCreateRequest credManRequest = mCredentialManager.getCreateRequest();
        Assert.assertNotNull(credManRequest);
        Assert.assertEquals(
                credManRequest.getOrigin(), Fido2CredentialRequest.convertOriginToString(mOrigin));
        Assert.assertEquals(
                credManRequest.getType(), "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        Assert.assertEquals(credManRequest.getCredentialData().getString(
                                    "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"),
                "{serialized_make_request}");
        Assert.assertTrue(credManRequest.getAlwaysSendAppInfoToProvider());
        Assert.assertTrue(
                credManRequest.getCandidateQueryData().containsKey("com.android.chrome.CHANNEL"));
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertFalse(mFido2ApiCallHelper.mMakeCredentialCalled);
    }

    @Test
    @SmallTest
    public void testMakeCredential_rkDiscouraged_goesToPlayServices() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCreationOptions.authenticatorSelection.residentKey = ResidentKeyRequirement.DISCOURAGED;
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertTrue(mFido2ApiCallHelper.mMakeCredentialCalled);
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabledUserCancel_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_USER_CANCELED", "Message"));
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
    }

    @Test
    @SmallTest
    public void testMakeCredential_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_UNKNOWN", "Message"));
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin,
                (responseStatus, response)
                        -> mCallback.onRegisterResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabled_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        Assert.assertNotNull(credManRequest);
        Assert.assertEquals(
                credManRequest.getOrigin(), Fido2CredentialRequest.convertOriginToString(mOrigin));
        Assert.assertEquals(credManRequest.getCredentialOptions().size(), 1);
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        Assert.assertNotNull(option);
        Assert.assertEquals(option.getType(), "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        Assert.assertEquals(option.getCredentialRetrievalData().getString(
                                    "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"),
                "{serialized_get_request}");
        Assert.assertTrue(option.getCandidateQueryData().containsKey("com.android.chrome.CHANNEL"));
        Assert.assertFalse(option.isSystemProviderRequired());
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 0);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledUserCancel_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_USER_CANCELED", "Message"));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 0);
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_UNKNOWN", "Message"));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 0);
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledSuccess_success() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        Assert.assertNotNull(credManRequest);
        Assert.assertEquals(
                credManRequest.getOrigin(), Fido2CredentialRequest.convertOriginToString(mOrigin));
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        Assert.assertNotNull(option);
        Assert.assertEquals(option.getType(), "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        Assert.assertEquals(option.getCredentialRetrievalData().getString(
                                    "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"),
                "{serialized_get_request}");
        Assert.assertFalse(option.isSystemProviderRequired());
        Assert.assertEquals(mCallback.getStatus(), null);
        Assert.assertEquals(mMockBrowserBridge.getOnCredManConditionalRequestPendingCallCount(), 1);
        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 0);
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;
        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_UNKNOWN", "Message"));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledRpCancelWhileIdle_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        mRequest.cancelConditionalGetAssertion(mFrameHost);
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ABORT_ERROR));
        Assert.assertEquals(mMockBrowserBridge.getCleanupRequestCallCount(), 1);
        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 0);
    }

    @Test
    @SmallTest
    public void
    testConditionalGetAssertion_credManEnabledUserCancelWhileIdle_DoesNotCancelConditionalRequest() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.GetCredentialException.TYPE_USER_CANCELED", "Message"));

        mMockBrowserBridge.getCredManGetAssertionCallback().onResult(true);

        Assert.assertEquals(mCallback.getStatus(), null);
        Assert.assertEquals(mMockBrowserBridge.getCleanupRequestCallCount(), 0);
        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 1);
    }

    @Test
    @SmallTest
    public void testConditionalGetAssertion_credManEnabledWithPasswords_canHavePasswordResponse() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        mRequestOptions.isConditional = true;

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));

        FakeAndroidCredManGetRequest credManRequest = mCredentialManager.getGetRequest();
        Assert.assertNotNull(credManRequest);
        Assert.assertEquals(credManRequest.getCredentialOptions().size(), 1);

        String username = "coolUserName";
        String password = "38kay5er1sp0r38";
        mCredentialManager.setCredManGetResponseCredential(
                new FakeAndroidPasswordCredential(username, password));
        mMockBrowserBridge.getCredManGetAssertionCallback().onResult(true);

        credManRequest = mCredentialManager.getGetRequest();
        Assert.assertNotNull(credManRequest);
        Assert.assertEquals(credManRequest.getCredentialOptions().size(), 2);
        List<FakeAndroidCredentialOption> credentialOptions = credManRequest.getCredentialOptions();
        Assert.assertEquals(credentialOptions.get(0).getType(),
                "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        Assert.assertEquals(
                credentialOptions.get(1).getType(), "android.credentials.TYPE_PASSWORD_CREDENTIAL");

        Assert.assertEquals(mMockBrowserBridge.getOnCredManClosedCallCount(), 0);
        // A password is selected, the callback will not be signed.
        Assert.assertEquals(mCallback.getStatus(), null);

        Assert.assertEquals(
                mMockBrowserBridge.getOnPasswordCredentialReceivedCall().get("username"), username);
        Assert.assertEquals(
                mMockBrowserBridge.getOnPasswordCredentialReceivedCall().get("password"), password);
    }

    static class FakeFido2ApiCallHelper extends Fido2ApiCallHelper {
        public boolean mMakeCredentialCalled;
        public boolean mGetAssertionCalled;

        @Override
        public boolean arePlayServicesAvailable() {
            return true;
        }

        @Override
        public void invokeFido2MakeCredential(PublicKeyCredentialCreationOptions options, Uri uri,
                byte[] clientDataHash, OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback) throws NoSuchAlgorithmException {
            mMakeCredentialCalled = true;
            // Don't make any actual calls to Play Services.
        }

        @Override
        public void invokeFido2GetAssertion(PublicKeyCredentialRequestOptions options, Uri uri,
                byte[] clientDataHash, OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback) {
            mGetAssertionCalled = true;
            // Don't make any actual calls to Play Services.
        }
    }
}
