// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.Collections;

/** Tests for {@link CurrentPageVerifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class CurrentPageVerifierTest {
    private static final Origin TRUSTED_ORIGIN = Origin.create("https://www.origin1.com/");
    private static final Origin OTHER_TRUSTED_ORIGIN = Origin.create("https://www.origin2.com/");
    private static final String TRUSTED_ORIGIN_PAGE1 = TRUSTED_ORIGIN + "/page1";
    private static final String OTHER_TRUSTED_ORIGIN_PAGE1 = OTHER_TRUSTED_ORIGIN + "/page1";
    private static final String UNTRUSTED_PAGE = "https://www.origin3.com/page1";
    private static final String UNTRUSTED_ORIGIN_BLOB_PAGE =
            "blob:https://www.origin3.com/1234-5678";
    private static final String TRUSTED_ORIGIN_BLOB_PAGE = "blob:" + TRUSTED_ORIGIN + "/1234-5678";
    private static final String ABOUT_BLANK_PAGE = "about:blank";
    private static final String DATA_PAGE = "data:text/html,<div>Test</div>";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock TabObserverRegistrar mTabObserverRegistrar;
    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock CustomTabActivityTabProvider mTabProvider;
    @Mock CustomTabIntentDataProvider mIntentDataProvider;
    @Mock Tab mTab;
    @Captor ArgumentCaptor<CustomTabTabObserver> mTabObserverCaptor;

    TestVerifier mVerifierDelegate = new TestVerifier();

    private CurrentPageVerifier mCurrentPageVerifier;

    @Before
    public void setUp() {
        when(mTabProvider.getTab()).thenReturn(mTab);
        doNothing()
                .when(mTabObserverRegistrar)
                .registerActivityTabObserver(mTabObserverCaptor.capture());
        when(mIntentDataProvider.getTrustedWebActivityAdditionalOrigins())
                .thenReturn(Collections.singletonList("https://www.origin2.com/"));
        mCurrentPageVerifier =
                new CurrentPageVerifier(
                        mTabProvider,
                        mIntentDataProvider,
                        mVerifierDelegate,
                        mTabObserverRegistrar,
                        mLifecycleDispatcher);
        // TODO(peconn): Add check on permission updated being updated.
    }

    @Test
    public void verifiesOriginOfInitialPage() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        verifyStartsVerification(TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    public void statusIsPending_UntilVerificationFinished() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        assertStatus(VerificationStatus.PENDING);
    }

    @Test
    public void statusIsSuccess_WhenVerificationSucceeds() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    @Test
    public void statusIsFail_WhenVerificationFails() {
        setInitialUrl(UNTRUSTED_PAGE);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.failVerification(Origin.create(UNTRUSTED_PAGE));
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void verifies_WhenNavigatingToOtherTrustedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        verifyStartsVerification(OTHER_TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    public void doesntUpdateState_IfVerificationFinishedAfterLeavingOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        navigateToUrl(UNTRUSTED_PAGE);
        mVerifierDelegate.failVerification(Origin.create(UNTRUSTED_PAGE));

        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void reverifiesOrigin_WhenReturningToIt_IfFirstVerificationDidntFinishInTime() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(Origin.create(OTHER_TRUSTED_ORIGIN_PAGE1));
        navigateToUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    /**
     * Tests that navigating from a verified page to a cross-origin blob: URL updates the
     * verification status to {@link VerificationStatus#FAILURE}.
     */
    @Test
    public void statusIsFail_WhenNavigatingToUntrustedOriginBlobUrl() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);

        navigateToUrl(UNTRUSTED_ORIGIN_BLOB_PAGE);
        assertStatus(VerificationStatus.FAILURE);
        assertEquals(UNTRUSTED_ORIGIN_BLOB_PAGE, mCurrentPageVerifier.getState().url);
    }

    /**
     * Tests that navigating to a blob: URL with the same origin as the verified app results in
     * {@link VerificationStatus#FAILURE}.
     *
     * <p>Note: Failing verification for a same-origin blob: URL is not the ideal desired platform
     * behavior (since the document belongs to the verified origin), but is an intentional fail-safe
     * trade-off of the current URL-based {@link Origin#create} implementation.
     */
    @Test
    public void statusIsFail_WhenNavigatingToSameOriginBlobUrl() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);

        navigateToUrl(TRUSTED_ORIGIN_BLOB_PAGE);
        assertStatus(VerificationStatus.FAILURE);
    }

    /**
     * Tests that navigating to about:blank results in {@link VerificationStatus#FAILURE}.
     *
     * <p>Note: Failing verification for a same-origin about:blank navigation is not necessarily the
     * desired behavior, but is an intentional fail-safe trade-off of the current URL-based {@link
     * Origin#create} implementation.
     */
    @Test
    public void statusIsFail_WhenNavigatingToAboutBlank() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);

        navigateToUrl(ABOUT_BLANK_PAGE);
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void statusIsFail_WhenNavigatingToDataUrl() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);

        navigateToUrl(DATA_PAGE);
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void statusIsFail_WhenInitialUrlHasNonHttpScheme() {
        setInitialUrl(UNTRUSTED_ORIGIN_BLOB_PAGE);
        mCurrentPageVerifier.onFinishNativeInitialization();
        assertStatus(VerificationStatus.FAILURE);
    }

    /**
     * Tests that a pending verification completing after navigating to a blob: URL does not update
     * the status to {@link VerificationStatus#SUCCESS}.
     */
    @Test
    public void doesntUpdateState_IfVerificationFinishedAfterNavigatingToBlobUrl() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        assertStatus(VerificationStatus.PENDING);

        navigateToUrl(UNTRUSTED_ORIGIN_BLOB_PAGE);
        assertStatus(VerificationStatus.FAILURE);

        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.FAILURE);
    }

    /**
     * Tests that navigating back to the verified origin restores {@link
     * VerificationStatus#SUCCESS}.
     */
    @Test
    public void statusIsSuccess_WhenNavigatingBackFromBlobUrl() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(UNTRUSTED_ORIGIN_BLOB_PAGE);
        assertStatus(VerificationStatus.FAILURE);

        navigateToUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    private void assertStatus(@CurrentPageVerifier.VerificationStatus int status) {
        shadowOf(Looper.getMainLooper()).idle();
        assertEquals(status, mCurrentPageVerifier.getState().status);
    }

    private void verifyStartsVerification(String url) {
        assertTrue(mVerifierDelegate.hasPendingVerification(Origin.create(url)));
    }

    private static GURL createMockGurl(String url) {
        GURL gurl = Mockito.mock(GURL.class);
        when(gurl.getSpec()).thenReturn(url);
        return gurl;
    }

    private void setInitialUrl(String url) {
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(url);
        // TODO(crbug.com/40549331): Pass in GURL.
        GURL gurl = createMockGurl(url);
        when(mTab.getUrl()).thenReturn(gurl);
    }

    private void navigateToUrl(String url) {
        GURL gurl = createMockGurl(url);
        when(mTab.getUrl()).thenReturn(gurl);
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        gurl,
                        /* isRendererInitiated= */ false,
                        /* transition= */ 0,
                        /* hasUserGesture= */ false);
        for (CustomTabTabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
        }

        navigation.callDidFinishForTesting(gurl);
        for (CustomTabTabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);
        }
    }
}
