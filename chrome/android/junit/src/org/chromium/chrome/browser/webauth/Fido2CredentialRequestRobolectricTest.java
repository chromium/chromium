// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.DeviceFeatureList;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

@RunWith(BaseRobolectricTestRunner.class)
public class Fido2CredentialRequestRobolectricTest {
    private FakeAndroidCredentialManager mCredentialManager;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private Fido2ApiTestHelper.AuthenticatorCallback mCallback;
    private Origin mOrigin;

    @Mock
    private RenderFrameHost mFrameHost;
    @Mock
    private WebContents mWebContents;
    @Mock
    GURLUtils.Natives mGURLUtilsJniMock;
    @Mock
    ClientDataJsonImpl.Natives mClientDataJsonImplMock;

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

        mCredentialManager = new FakeAndroidCredentialManager();
        mRequest.setOverrideVersionCheckForTesting(true);
        mRequest.setCredManClassesForTesting(mCredentialManager,
                FakeAndroidCredManCreateRequest.Builder.class,
                FakeAndroidCredManGetRequest.Builder.class,
                FakeAndroidCredentialOption.Builder.class);
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
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
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
        FakeAndroidCredentialOption option = credManRequest.getCredentialOptions().get(0);
        Assert.assertNotNull(option);
        Assert.assertEquals(option.getType(), "androidx.credentials.TYPE_PUBLIC_KEY_CREDENTIAL");
        Assert.assertEquals(option.getCredentialRetrievalData().getString(
                                    "androidx.credentials.BUNDLE_KEY_REQUEST_JSON"),
                "{serialized_get_request}");
        Assert.assertFalse(option.isSystemProviderRequired());
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledUserCancel_notAllowedError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_USER_CANCELED", "Message"));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
    }

    @Test
    @SmallTest
    public void testGetAssertion_credManEnabledUnknownError_unknownError() {
        // Calls to `context.getMainExecutor()` require API level 28 or higher.
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);

        mCredentialManager.setErrorResponse(new FakeAndroidCredManException(
                "android.credentials.CreateCredentialException.TYPE_UNKNOWN", "Message"));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, /*payment=*/null,
                (responseStatus, response)
                        -> mCallback.onSignResponse(responseStatus, response),
                errorStatus -> mCallback.onError(errorStatus));
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
    }
}
