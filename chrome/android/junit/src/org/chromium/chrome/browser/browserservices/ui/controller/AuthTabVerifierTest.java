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

import static org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier.RESULT_VERIFICATION_FAILED;
import static org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier.RESULT_VERIFICATION_TIMED_OUT;
import static org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier.VERIFICATION_TIMEOUT_MS;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
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

import java.util.concurrent.TimeUnit;

/** Tests for {@link AuthTabVerifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
@Config(shadows = {ShadowPostTask.class, ShadowSystemClock.class})
public class AuthTabVerifierTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    private Runnable mDelayedTask;

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl((taskTraits, task, delay) -> mDelayedTask = task);

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
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "CustomTabs.AuthTab.TimeToDalVerification.SinceStart", 1000)
                        .build();

        String url = REDIRECT_URL;
        mDelegate.onFinishNativeInitialization();

        ShadowSystemClock.advanceBy(1000, TimeUnit.MILLISECONDS);
        simulateVerificationResultFromNetwork(url, true);

        mDelegate.returnAsActivityResult(new GURL(url));
        verify(mActivity).setResult(eq(Activity.RESULT_OK), any());
        verify(mActivity).finish();
        histograms.assertExpected();
    }

    @Test
    public void validatedHttpsReturnsResult_failure() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("CustomTabs.AuthTab.TimeToDalVerification.SinceStart", 300)
                        .build();
        String url = REDIRECT_URL;
        mDelegate.onFinishNativeInitialization();

        ShadowSystemClock.advanceBy(300, TimeUnit.MILLISECONDS);
        simulateVerificationResultFromNetwork(url, false);

        mDelegate.returnAsActivityResult(new GURL(url));
        verify(mActivity).setResult(eq(RESULT_VERIFICATION_FAILED), any());
        verify(mActivity).finish();
        histograms.assertExpected();
    }

    @Test
    public void returnsResultLaterForDelayedNetworkResponse() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "CustomTabs.AuthTab.TimeToDalVerification.SinceStart", 5300)
                        .expectIntRecord(
                                "CustomTabs.AuthTab.TimeToDalVerification.SinceFlowCompletion", 300)
                        .build();

        String url = REDIRECT_URL;
        GURL gurl = new GURL(url);
        mDelegate.onFinishNativeInitialization();
        assertTrue(mDelegate.shouldRedirectHttpsAuthUrl(gurl));

        ShadowSystemClock.advanceBy(5000, TimeUnit.MILLISECONDS);
        // User started auth process before receiving the verification result from the network.
        mDelegate.returnAsActivityResult(gurl);
        assertFalse(mDelegate.hasValidatedHttps());

        verify(mActivity, never()).setResult(anyInt(), any());
        verify(mActivity, never()).finish();

        ShadowSystemClock.advanceBy(300, TimeUnit.MILLISECONDS);
        simulateVerificationResultFromNetwork(url, true);

        verify(mActivity).setResult(eq(Activity.RESULT_OK), any());
        verify(mActivity).finish();
        histograms.assertExpected();
    }

    @Test
    public void returnsResultLaterForDelayedNetworkResponse_timeout() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("CustomTabs.AuthTab.TimeToDalVerification.SinceStart")
                        .expectAnyRecord(
                                "CustomTabs.AuthTab.TimeToDalVerification.SinceFlowCompletion")
                        .build();
        String url = REDIRECT_URL;
        GURL gurl = new GURL(url);
        mDelegate.onFinishNativeInitialization();
        assertTrue(mDelegate.shouldRedirectHttpsAuthUrl(gurl));

        ShadowSystemClock.advanceBy(1000, TimeUnit.MILLISECONDS);
        // User started auth process before receiving the verification result from the network.
        mDelegate.returnAsActivityResult(gurl);
        assertFalse(mDelegate.hasValidatedHttps());

        verify(mActivity, never()).setResult(anyInt(), any());
        verify(mActivity, never()).finish();

        ShadowSystemClock.advanceBy(VERIFICATION_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        // Simulate timeout.
        mDelayedTask.run();

        verify(mActivity).setResult(eq(RESULT_VERIFICATION_TIMED_OUT), any());
        verify(mActivity).finish();
        histograms.assertExpected();
    }
}
