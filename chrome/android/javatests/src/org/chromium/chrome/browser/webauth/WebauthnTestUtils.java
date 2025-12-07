// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.ConditionVariable;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.junit.Assert;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.blink.mojom.CredentialInfo;
import org.chromium.blink.mojom.CredentialType;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.components.webauthn.AssertionMediationType;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.Fido2ApiTestHelper;
import org.chromium.components.webauthn.FidoIntentSender;
import org.chromium.components.webauthn.InternalAuthenticator;
import org.chromium.components.webauthn.NonCredentialReturnReason;
import org.chromium.components.webauthn.WebauthnBrowserBridge;
import org.chromium.components.webauthn.WebauthnCredentialDetails;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.RenderFrameHost.WebAuthSecurityChecksResults;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.mojo_base.mojom.String16;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

/** Shared test utilities and mocks for WebAuthn unit tests. */
public class WebauthnTestUtils {
    private static final String TAG = "WebauthnTestUtils";

    private static final String FIDO_OVERRIDE_COMMAND =
            "su root am broadcast -a com.google.android.gms.phenotype.FLAG_OVERRIDE --es package"
                    + " com.google.android.gms.fido --es user * --esa flags"
                    + " Fido2ApiKnownBrowsers__fingerprints --esa values"
                    + " %s --esa types"
                    + " string --ez commit true --user 0 com.google.android.gms";

    public static class MockIntentSender implements FidoIntentSender {
        private Pair<Integer, Intent> mNextResult;
        private boolean mInvokeCallbackImmediately = true;
        private Callback<Pair<Integer, Intent>> mCallback;
        private final ConditionVariable mShowIntentCalled = new ConditionVariable();

        public void setNextResult(int responseCode, Intent intent) {
            mNextResult = new Pair<>(responseCode, intent);
        }

        public void setNextResultIntent(Intent intent) {
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
                assertThat(mCallback).isNull();
                mCallback = callback;
            }
            mShowIntentCalled.open();
            return true;
        }
    }

    public static class MockFido2ApiCallHelper extends Fido2ApiCallHelper {
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

        @Override
        public void invokePasskeyCacheGetCredentials(
                AuthenticationContextProvider authenticationContextProvider,
                String relyingParty,
                OnSuccessListener<List<WebauthnCredentialDetails>> successListener,
                OnFailureListener failureListener) {
            if (mInvokeCallbackImmediately) {
                successListener.onSuccess(mReturnedCredentialDetails);
                return;
            }
            mSuccessCallback = successListener;
        }

        @Override
        public boolean arePlayServicesAvailable() {
            return true;
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

    public static class MockIncognitoWebContents extends MockWebContents {
        @Override
        public boolean isIncognito() {
            return true;
        }
    }

    public static class TestAuthenticatorImplJni implements InternalAuthenticator.Natives {
        private final Fido2ApiTestHelper.AuthenticatorCallback mCallback;

        public TestAuthenticatorImplJni(Fido2ApiTestHelper.AuthenticatorCallback callback) {
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
            mCallback.onSignResponseWithStatus(
                    status,
                    byteBuffer == null
                            ? null
                            : GetAssertionAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                long nativeInternalAuthenticator, boolean isUvpaa) {}

        @Override
        public void invokeGetMatchingCredentialIdsResponse(
                long nativeInternalAuthenticator, byte[][] matchingCredentials) {}
    }

    public static class MockBrowserBridge extends WebauthnBrowserBridge {
        public enum CallbackInvocationType {
            IMMEDIATE_PASSKEY,
            IMMEDIATE_PASSWORD,
            IMMEDIATE_HYBRID,
            IMMEDIATE_REJECT_IMMEDIATE,
            USER_DISMISSED_UI,
            ERROR,
            DELAYED
        }

        private List<WebauthnCredentialDetails> mExpectedCredentialList;
        private CallbackInvocationType mInvokeCallbackImmediately =
                CallbackInvocationType.IMMEDIATE_PASSKEY;
        private Callback<SelectedCredential> mCredentialCallback;
        private Runnable mHybridCallback;
        private Callback<Integer> mNonCredentialCallback;
        private int mCleanupCalled;

        private final String16 mPasswordCredUsername16;
        private final String16 mPasswordCredPassword16;

        public MockBrowserBridge(String16 username, String16 password) {
            mPasswordCredUsername16 = username;
            mPasswordCredPassword16 = password;
        }

        public MockBrowserBridge() {
            this(null, null);
        }

        @Override
        public void onCredentialsDetailsListReceived(
                RenderFrameHost frameHost,
                List<WebauthnCredentialDetails> credentialList,
                @AssertionMediationType int mediationType,
                Callback<SelectedCredential> credentialCallback,
                @Nullable Runnable hybridCallback,
                Callback<Integer> nonCredentialCallback) {
            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_PASSKEY
                    || mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_PASSWORD) {
                Assert.assertEquals(mExpectedCredentialList.size(), credentialList.size());
                for (int i = 0; i < credentialList.size(); i++) {
                    Assert.assertEquals(
                            mExpectedCredentialList.get(i).mUserName,
                            credentialList.get(i).mUserName);
                    Assert.assertEquals(
                            mExpectedCredentialList.get(i).mUserDisplayName,
                            credentialList.get(i).mUserDisplayName);
                    Assert.assertArrayEquals(
                            mExpectedCredentialList.get(i).mCredentialId,
                            credentialList.get(i).mCredentialId);
                    Assert.assertArrayEquals(
                            mExpectedCredentialList.get(i).mUserId, credentialList.get(i).mUserId);
                    Assert.assertEquals(
                            mExpectedCredentialList.get(i).mLastUsedTimeMs,
                            credentialList.get(i).mLastUsedTimeMs);
                }
            }

            mCredentialCallback = credentialCallback;
            mHybridCallback = hybridCallback;
            mNonCredentialCallback = nonCredentialCallback;

            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_PASSKEY) {
                invokePasskeyCallback();
            }

            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_PASSWORD) {
                invokePasswordCallback();
            }

            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_HYBRID) {
                invokeHybridCallback();
            }

            if (mInvokeCallbackImmediately == CallbackInvocationType.IMMEDIATE_REJECT_IMMEDIATE) {
                invokeNonCredentialCallback(NonCredentialReturnReason.IMMEDIATE_NO_CREDENTIALS);
            }

            if (mInvokeCallbackImmediately == CallbackInvocationType.USER_DISMISSED_UI) {
                invokeNonCredentialCallback(NonCredentialReturnReason.USER_DISMISSED);
            }

            if (mInvokeCallbackImmediately == CallbackInvocationType.ERROR) {
                invokeNonCredentialCallback(NonCredentialReturnReason.ERROR);
            }
        }

        @Override
        public void cleanupRequest(RenderFrameHost frameHost) {
            mCleanupCalled++;
        }

        public void setExpectedCredentialDetailsList(
                List<WebauthnCredentialDetails> credentialList) {
            mExpectedCredentialList = credentialList;
        }

        public void setInvokeCallbackImmediately(CallbackInvocationType type) {
            mInvokeCallbackImmediately = type;
        }

        public void invokePasskeyCallback() {
            if (mExpectedCredentialList.isEmpty()) {
                mNonCredentialCallback.onResult(NonCredentialReturnReason.ERROR);
                return;
            }
            mCredentialCallback.onResult(
                    new WebauthnBrowserBridge.SelectedCredential(
                            mExpectedCredentialList.get(0).mCredentialId));
        }

        public void invokePasswordCallback() {
            CredentialInfo passwordCredential = new CredentialInfo();
            passwordCredential.type = CredentialType.PASSWORD;
            passwordCredential.name = mPasswordCredUsername16;
            passwordCredential.id = mPasswordCredUsername16;
            passwordCredential.password = mPasswordCredPassword16;
            mCredentialCallback.onResult(
                    new WebauthnBrowserBridge.SelectedCredential(passwordCredential));
        }

        public void invokeHybridCallback() {
            mHybridCallback.run();
        }

        public void invokeNonCredentialCallback(Integer reason) {
            mNonCredentialCallback.onResult(reason);
        }

        public int getCleanupCalledCount() {
            return mCleanupCalled;
        }
    }

    public static class MockAuthenticatorRenderFrameHost extends MockRenderFrameHost {
        private GURL mLastUrl;
        private boolean mIsPaymentCredentialCreation;

        public MockAuthenticatorRenderFrameHost() {}

        @Override
        public GURL getLastCommittedURL() {
            return mLastUrl;
        }

        @Override
        public Origin getLastCommittedOrigin() {
            return Origin.create(mLastUrl);
        }

        public void setLastCommittedUrl(GURL url) {
            mLastUrl = url;
        }

        public boolean isPaymentCredentialCreation() {
            return mIsPaymentCredentialCreation;
        }

        @Override
        public void performMakeCredentialWebAuthSecurityChecks(
                String relyingPartyId,
                Origin effectiveOrigin,
                boolean isPaymentCredentialCreation,
                @Nullable Origin remoteDesktopClientOverrideOrigin,
                Callback<WebAuthSecurityChecksResults> callback) {
            mIsPaymentCredentialCreation = isPaymentCredentialCreation;
            super.performMakeCredentialWebAuthSecurityChecks(
                    relyingPartyId,
                    effectiveOrigin,
                    isPaymentCredentialCreation,
                    remoteDesktopClientOverrideOrigin,
                    callback);
        }
    }

    public static void assertHasAttestation(MakeCredentialAuthenticatorResponse response) {
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

    public static void applyFidoOverride(Context context) {
        String fingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(context.getPackageName())
                        .stream()
                        .map(s -> s.replaceAll(":", ""))
                        .collect(Collectors.joining("\'"));
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .executeShellCommand(String.format(FIDO_OVERRIDE_COMMAND, fingerprints));
        Log.d(
                TAG,
                "Executing command: '"
                        + String.format(WebauthnTestUtils.FIDO_OVERRIDE_COMMAND, fingerprints)
                        + "'");
    }

    private WebauthnTestUtils() {}
}
