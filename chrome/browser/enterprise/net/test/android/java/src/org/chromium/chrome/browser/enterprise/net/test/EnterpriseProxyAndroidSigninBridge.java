// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.net.test;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test-only JNI bridge installing a controllable {@link FakeAccountManagerFacade} so Android
 * EnterpriseProxy browsertests exercise the real {@code ProfileOAuth2TokenServiceDelegateAndroid}
 * and OS-mediated {@code AccountManagerFacade} path end-to-end without {@code
 * IdentityTestEnvironmentProfileAdaptor}.
 */
@NullMarked
@JNINamespace("enterprise::test")
final class EnterpriseProxyAndroidSigninBridge {
    private static final String PROXY_OAUTH_SCOPE_SUBSTRING = "cloud-secure-gateway-users.access";

    private static class TestAccountManagerFacade extends FakeAccountManagerFacade {
        private volatile @Nullable String mOverrideAccessToken;
        private final AtomicInteger mTokenRequestCount = new AtomicInteger(0);
        // Scope simulated token errors to the Secure Gateway OAuth2 scope (matching
        // `DesktopTestAuthHelper::OnAccessTokenRequested()`) so background Chrome non-proxy token
        // fetches (e.g. `AccountInfoFetcherGaia`) do not pre-poison `IdentityManager` with a
        // persistent error before the proxy HTTP 407 challenge occurs.
        private final Map<CoreAccountId, GoogleServiceAuthError> mProxyAccessTokenErrors =
                Collections.synchronizedMap(new HashMap<>());

        TestAccountManagerFacade(String defaultAccessToken) {
            super();
            mOverrideAccessToken = defaultAccessToken;
        }

        void setOverrideAccessToken(String accessToken) {
            mOverrideAccessToken = accessToken;
        }

        int getTokenRequestCount() {
            return mTokenRequestCount.get();
        }

        void setOrClearAccessTokenError(CoreAccountId accountId, int authErrorState) {
            if (authErrorState == GoogleServiceAuthErrorState.NONE) {
                mProxyAccessTokenErrors.remove(accountId);
            } else {
                mProxyAccessTokenErrors.put(accountId, new GoogleServiceAuthError(authErrorState));
            }
        }

        @Override
        public void getAccessToken(
                CoreAccountInfo coreAccountInfo, String scope, GetAccessTokenCallback callback) {
            if (scope.contains(PROXY_OAUTH_SCOPE_SUBSTRING)) {
                mTokenRequestCount.incrementAndGet();
                @Nullable GoogleServiceAuthError authError =
                        mProxyAccessTokenErrors.get(coreAccountInfo.getId());
                if (authError != null) {
                    ThreadUtils.postOnUiThread(() -> callback.onGetTokenFailure(authError));
                    return;
                }
            }
            super.getAccessToken(
                    coreAccountInfo,
                    scope,
                    new GetAccessTokenCallback() {
                        @Override
                        public void onGetTokenSuccess(AccessTokenData token) {
                            String overrideToken = mOverrideAccessToken;
                            if (overrideToken != null && !overrideToken.isEmpty()) {
                                callback.onGetTokenSuccess(new AccessTokenData(overrideToken));
                            } else {
                                callback.onGetTokenSuccess(token);
                            }
                        }

                        @Override
                        public void onGetTokenFailure(GoogleServiceAuthError authError) {
                            callback.onGetTokenFailure(authError);
                        }
                    });
        }
    }

    private static @Nullable TestAccountManagerFacade sFacade;

    private EnterpriseProxyAndroidSigninBridge() {}

    @CalledByNative
    static void installFakeAccountManagerFacade(@JniType("std::string") String defaultAccessToken) {
        sFacade = new TestAccountManagerFacade(defaultAccessToken);
        AccountManagerFacadeProvider.setInstanceForTests(sFacade);
    }

    @CalledByNative
    @SuppressWarnings("NullAway") // sInstanceForTesting accepts null to clear the test override.
    static void resetFakeAccountManagerFacade() {
        sFacade = null;
        AccountManagerFacadeProvider.setInstanceForTests(null);
    }

    @CalledByNative
    static void addAccount(
            @JniType("std::string") String email,
            @JniType("std::string") String gaiaId,
            @JniType("std::string") String hostedDomain) {
        TestAccountManagerFacade facade = sFacade;
        assert facade != null;
        AccountInfo accountInfo =
                new AccountInfo.Builder(email, new GaiaId(gaiaId))
                        .fullName("Full Name")
                        .givenName("Given Name")
                        .hostedDomain(hostedDomain)
                        .build();
        facade.addAccount(accountInfo);
    }

    @CalledByNative
    static void setAccessTokenError(@JniType("std::string") String gaiaId, int authErrorState) {
        TestAccountManagerFacade facade = sFacade;
        assert facade != null;
        facade.setOrClearAccessTokenError(new CoreAccountId(new GaiaId(gaiaId)), authErrorState);
    }

    @CalledByNative
    static void setOverrideAccessToken(@JniType("std::string") String accessToken) {
        TestAccountManagerFacade facade = sFacade;
        assert facade != null;
        facade.setOverrideAccessToken(accessToken);
    }

    @CalledByNative
    static int getTokenRequestCount() {
        TestAccountManagerFacade facade = sFacade;
        return facade != null ? facade.getTokenRequestCount() : 0;
    }
}
