// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.Collections;

/**
 * Tests for {@link CurrentPageVerifier}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@DisableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_POST_MESSAGE)
public class CurrentPageVerifierTest {
    private static final Origin TRUSTED_ORIGIN = Origin.create("https://www.origin1.com/");
    private static final Origin OTHER_TRUSTED_ORIGIN = Origin.create("https://www.origin2.com/");
    private static final String TRUSTED_ORIGIN_PAGE1 = TRUSTED_ORIGIN + "/page1";
    private static final String OTHER_TRUSTED_ORIGIN_PAGE1 = OTHER_TRUSTED_ORIGIN + "/page1";
    private static final String UNTRUSTED_PAGE = "https://www.origin3.com/page1";

    public static final String PACKAGE_NAME = "package.name";

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    TabObserverRegistrar mTabObserverRegistrar;
    @Mock
    ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock
    CustomTabActivityTabProvider mTabProvider;
    @Mock
    CustomTabIntentDataProvider mIntentDataProvider;
    @Mock
    Tab mTab;
    @Mock
    ClientPackageNameProvider mClientPackageNameProvider;
    @Captor
    ArgumentCaptor<CustomTabTabObserver> mTabObserverCaptor;

    TestVerifier mVerifierDelegate = new TestVerifier();

    private CurrentPageVerifier mCurrentPageVerifier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTabProvider.getTab()).thenReturn(mTab);
        when(mClientPackageNameProvider.get()).thenReturn(PACKAGE_NAME);
        doNothing()
                .when(mTabObserverRegistrar)
                .registerActivityTabObserver(mTabObserverCaptor.capture());
        when(mIntentDataProvider.getTrustedWebActivityAdditionalOrigins())
                .thenReturn(Collections.singletonList("https://www.origin2.com/"));
        mCurrentPageVerifier = new CurrentPageVerifier(mLifecycleDispatcher, mTabObserverRegistrar,
                mTabProvider, mIntentDataProvider, mVerifierDelegate);
        // TODO(peconn): Add check on permission updated being updated.
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void verifiesOriginOfInitialPage() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        verifyStartsVerification(TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void statusIsPending_UntilVerificationFinished() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        assertStatus(VerificationStatus.PENDING);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void statusIsSuccess_WhenVerificationSucceeds() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void statusIsFail_WhenVerificationFails() {
        setInitialUrl(UNTRUSTED_PAGE);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.failVerification(Origin.create(UNTRUSTED_PAGE));
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void verifies_WhenNavigatingToOtherTrustedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        verifyStartsVerification(OTHER_TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void doesntUpdateState_IfVerificationFinishedAfterLeavingOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        navigateToUrl(UNTRUSTED_PAGE);
        mVerifierDelegate.failVerification(Origin.create(UNTRUSTED_PAGE));

        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void reverifiesOrigin_WhenReturningToIt_IfFirstVerificationDidntFinishInTime() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mCurrentPageVerifier.onFinishNativeInitialization();
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(Origin.create(OTHER_TRUSTED_ORIGIN_PAGE1));
        navigateToUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(Origin.create(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    private void assertStatus(@CurrentPageVerifier.VerificationStatus int status) {
        CriteriaHelper.pollUiThreadNested(
                () -> { return status == mCurrentPageVerifier.getState().status; });
    }

    private void verifyStartsVerification(String url) {
        assertTrue(mVerifierDelegate.hasPendingVerification(Origin.create(url)));
    }

    private void setInitialUrl(String url) {
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(url);
        when(mTab.getUrlString()).thenReturn(url);
    }

    private void navigateToUrl(String url) {
        GURL gurl = new GURL(url);
        when(mTab.getUrlString()).thenReturn(url);
        NavigationHandle navigation =
                new NavigationHandle(0 /* navigationHandleProxy */, gurl, true /* isMainFrame */,
                        false /* isSameDocument */, false /* isRendererInitiated */);
        for (CustomTabTabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidStartNavigation(mTab, navigation);
        }

        navigation.didFinish(gurl, false /* isErrorPage */, true /* hasCommitted */,
                false /* isFragmentNavigation */, false /* isDownload */,
                false /* isValidSearchFormUrl */, 0 /* pageTransition */, 0 /* errorCode*/,
                200 /* httpStatusCode*/);
        for (CustomTabTabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidFinishNavigation(mTab, navigation);
        }
    }
}
