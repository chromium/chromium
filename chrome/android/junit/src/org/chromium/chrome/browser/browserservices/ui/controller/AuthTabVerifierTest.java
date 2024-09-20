// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.customtabs.AuthTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.url.GURL;

/** Tests for {@link AuthTabVerifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AuthTabVerifierTest {
    private static final String REDIRECT_HOST = "www.awesome-site.com";
    private static final String REDIRECT_PATH = "/auth-response.html";
    private static final String REDIRECT_URL =
            UrlConstants.HTTPS_URL_PREFIX + REDIRECT_HOST + REDIRECT_PATH;
    private static final String OTHER_URL = "https://www.notverifiedurl.com/random_page.html";

    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock AuthTabIntentDataProvider mIntentDataProvider;
    @Mock ChromeOriginVerifierFactory mOriginVerifierFactory;
    @Mock ChromeOriginVerifier mOriginVerifier;
    @Mock CustomTabActivityTabProvider mActivityTabProvider;
    @Mock ExternalAuthUtils mExternalAuthUtils;
    @Mock Activity mActivity;

    private AuthTabVerifier mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mIntentDataProvider.getAuthRedirectHost()).thenReturn(REDIRECT_HOST);
        when(mIntentDataProvider.getAuthRedirectPath()).thenReturn(REDIRECT_PATH);
        when(mIntentDataProvider.getClientPackageName()).thenReturn("org.chromium.authtab");
        when(mOriginVerifierFactory.create(anyString(), anyInt(), any(), any()))
                .thenReturn(mOriginVerifier);

        mDelegate =
                new AuthTabVerifier(
                        mLifecycleDispatcher,
                        mActivityTabProvider,
                        mIntentDataProvider,
                        mOriginVerifierFactory,
                        mActivity,
                        mExternalAuthUtils);
    }

    void simulateVerificationResultFromNetwork(String url, boolean success) {
        // Simulate the OriginVerifier comes back with the verification result.
        ArgumentCaptor<OriginVerificationListener> verifyCallback =
                ArgumentCaptor.forClass(OriginVerificationListener.class);
        verify(mOriginVerifier).start(verifyCallback.capture(), eq(Origin.create(url)));
        verifyCallback.getValue().onOriginVerified(null, null, success, true);
        assertTrue(mDelegate.hasValidatedHttps());
    }

    @Test
    public void shouldRedirectHttpsAuthUrl() {
        mDelegate.onFinishNativeInitialization();
        verify(mOriginVerifier).start(any(), eq(Origin.create(REDIRECT_URL)));

        assertFalse(
                "URL should not be redirected",
                mDelegate.shouldRedirectHttpsAuthUrl(new GURL(OTHER_URL)));
        assertTrue(
                "URL should be redirected",
                mDelegate.shouldRedirectHttpsAuthUrl(new GURL(REDIRECT_URL)));
    }

    @Test
    public void validatedHttpsReturnsResult_success() {
        String url = REDIRECT_URL;
        mDelegate.onFinishNativeInitialization();

        simulateVerificationResultFromNetwork(url, true);

        mDelegate.returnAsActivityResult(new GURL(url));
        verify(mActivity).setResult(eq(Activity.RESULT_OK), any());
        verify(mActivity).finish();
    }

    @Test
    public void validatedHttpsReturnsResult_failure() {
        String url = REDIRECT_URL;
        mDelegate.onFinishNativeInitialization();

        simulateVerificationResultFromNetwork(url, false);

        mDelegate.returnAsActivityResult(new GURL(url));
        verify(mActivity).setResult(eq(AuthTabVerifier.RESULT_VERIFICATION_FAILED), any());
        verify(mActivity).finish();
    }

    @Test
    public void returnsResultLaterForDelayedNetworkResponse() {
        String url = REDIRECT_URL;
        GURL gurl = new GURL(url);
        mDelegate.onFinishNativeInitialization();
        assertTrue(mDelegate.shouldRedirectHttpsAuthUrl(gurl));

        // User started auth process before receiving the verification result from the network.
        mDelegate.returnAsActivityResult(gurl);
        assertFalse(mDelegate.hasValidatedHttps());

        verify(mActivity, never()).setResult(anyInt(), any());
        verify(mActivity, never()).finish();

        simulateVerificationResultFromNetwork(url, true);

        verify(mActivity).setResult(eq(Activity.RESULT_OK), any());
        verify(mActivity).finish();
    }
}
