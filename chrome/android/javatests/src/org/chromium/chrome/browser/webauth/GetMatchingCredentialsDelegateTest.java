// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.os.ConditionVariable;

import androidx.test.filters.SmallTest;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.FidoIntentSender;
import org.chromium.components.webauthn.GetMatchingCredentialIdsDelegate;
import org.chromium.components.webauthn.GetMatchingCredentialIdsDelegate.ResponseCallback;
import org.chromium.components.webauthn.GmsCoreUtils;
import org.chromium.components.webauthn.WebauthnCredentialDetails;
import org.chromium.components.webauthn.WebauthnRequestCallback;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link GetMatchingCredentialIdsDelegate}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors",
})
@Batch(Batch.PER_CLASS)
@Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_19W13)
public class GetMatchingCredentialsDelegateTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AuthenticationContextProvider mAuthenticationContextProvider;
    private GetMatchingCredentialIdsCallback mCallback;
    private MockFido2ApiCallHelper mFido2ApiCallHelper;

    private static final byte[] CREDENTIAL_ID_1 = new byte[] {1, 11, 101};
    private static final byte[] CREDENTIAL_ID_2 = new byte[] {2, 22, 102};
    private static final byte[] CREDENTIAL_ID_3 = new byte[] {3, 33, 103};
    private static final byte[] CREDENTIAL_ID_4 = new byte[] {4, 44, 104};
    private static final byte[] CREDENTIAL_ID_5 = new byte[] {5, 55, 105};

    private static List<WebauthnCredentialDetails> getAllCredentials() {
        WebauthnCredentialDetails credential1 = Fido2ApiTestHelper.getCredentialDetails();
        credential1.mCredentialId = CREDENTIAL_ID_1;
        credential1.mIsPayment = true;

        WebauthnCredentialDetails credential2 = Fido2ApiTestHelper.getCredentialDetails();
        credential2.mCredentialId = CREDENTIAL_ID_2;
        credential2.mIsPayment = false;

        WebauthnCredentialDetails credential3 = Fido2ApiTestHelper.getCredentialDetails();
        credential3.mCredentialId = CREDENTIAL_ID_3;
        credential3.mIsPayment = true;

        WebauthnCredentialDetails credential4 = Fido2ApiTestHelper.getCredentialDetails();
        credential4.mCredentialId = CREDENTIAL_ID_4;
        credential4.mIsPayment = false;
        return Arrays.asList(credential1, credential2, credential3, credential4);
    }

    private static class MockFido2ApiCallHelper extends Fido2ApiCallHelper {
        private List<WebauthnCredentialDetails> mReturnedCredentialDetails;

        @Override
        public void invokeFido2GetCredentials(
                AuthenticationContextProvider authenticationContextProvider,
                String relyingPartyId,
                OnSuccessListener<List<WebauthnCredentialDetails>> successCallback,
                OnFailureListener failureCallback) {
            successCallback.onSuccess(mReturnedCredentialDetails);
        }

        @Override
        public boolean arePlayServicesAvailable() {
            return true;
        }

        public void setReturnedCredentialDetails(List<WebauthnCredentialDetails> details) {
            mReturnedCredentialDetails = details;
        }
    }

    private static class GetMatchingCredentialIdsCallback implements ResponseCallback {
        private final ConditionVariable mCv = new ConditionVariable();
        private List<byte[]> mResponse;

        @Override
        public void onResponse(List<byte[]> matchingCredentialIds) {
            mResponse = matchingCredentialIds;
            mCv.open();
        }

        public void blockUntilCalled() {
            mCv.block();
        }

        public List<byte[]> getResponse() {
            return mResponse;
        }
    }

    @Before
    public void setUp() throws Exception {
        mAuthenticationContextProvider =
                new AuthenticationContextProvider() {
                    @Override
                    public android.content.Context getContext() {
                        return ContextUtils.getApplicationContext();
                    }

                    @Override
                    public RenderFrameHost getRenderFrameHost() {
                        return null;
                    }

                    @Override
                    public FidoIntentSender getIntentSender() {
                        return null;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return null;
                    }

                    @Override
                    public WebauthnRequestCallback getRequestCallback() {
                        return null;
                    }
                };
        mCallback = new GetMatchingCredentialIdsCallback();
        mFido2ApiCallHelper = new MockFido2ApiCallHelper();
        Fido2ApiCallHelper.overrideInstanceForTesting(mFido2ApiCallHelper);

        ExternalAuthUtils.setInstanceForTesting(
                new ExternalAuthUtils() {
                    @Override
                    public boolean canUseGooglePlayServices(
                            UserRecoverableErrorHandler errorHandler) {
                        return true;
                    }
                });
        GmsCoreUtils.setGmsCoreVersionForTesting(223300000);
    }

    @After
    public void tearDown() {
        GmsCoreUtils.setGmsCoreVersionForTesting(0);
    }

    @Test
    @SmallTest
    public void testGetMatchingCredentialIds_success() {
        String relyingPartyId = "subdomain.example.test";
        byte[][] allowCredentialIds =
                new byte[][] {
                    CREDENTIAL_ID_1, CREDENTIAL_ID_4, CREDENTIAL_ID_5,
                };

        mFido2ApiCallHelper.setReturnedCredentialDetails(getAllCredentials());

        GetMatchingCredentialIdsDelegate.getInstance()
                .getMatchingCredentialIds(
                        mAuthenticationContextProvider,
                        relyingPartyId,
                        allowCredentialIds,
                        /* requireThirdPartyPayment= */ false,
                        mCallback);
        mCallback.blockUntilCalled();
        Assert.assertArrayEquals(
                mCallback.getResponse().toArray(), new byte[][] {CREDENTIAL_ID_1, CREDENTIAL_ID_4});
    }

    @Test
    @SmallTest
    public void testGetMatchingCredentialIds_requireThirdPartyBit() {
        String relyingPartyId = "subdomain.example.test";
        byte[][] allowCredentialIds =
                new byte[][] {CREDENTIAL_ID_1, CREDENTIAL_ID_4, CREDENTIAL_ID_5};
        boolean requireThirdPartyPayment = true;

        mFido2ApiCallHelper.setReturnedCredentialDetails(getAllCredentials());

        GetMatchingCredentialIdsDelegate.getInstance()
                .getMatchingCredentialIds(
                        mAuthenticationContextProvider,
                        relyingPartyId,
                        allowCredentialIds,
                        /* requireThirdPartyPayment= */ true,
                        mCallback);
        mCallback.blockUntilCalled();
        Assert.assertArrayEquals(mCallback.getResponse().toArray(), new byte[][] {CREDENTIAL_ID_1});
    }
}
