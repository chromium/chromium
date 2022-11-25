// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for {@link QualityEnforcer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
@EnableFeatures({ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT,
        ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_WARNING})
@DisableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED)
public class QualityEnforcerUnitTest {
    private static final GURL TRUSTED_ORIGIN_PAGE = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL UNTRUSTED_PAGE = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
    private static final int HTTP_STATUS_SUCCESS = 200;
    private static final int HTTP_ERROR_NOT_FOUND = 404;

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private ChromeActivity mActivity;
    @Mock
    ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock
    private CustomTabIntentDataProvider mIntentDataProvider;
    @Mock
    private CustomTabsConnection mCustomTabsConnection;
    @Mock
    private Verifier mVerifier;
    @Mock
    private ClientPackageNameProvider mClientPackageNameProvider;
    @Mock
    private TabObserverRegistrar mTabObserverRegistrar;
    @Captor
    private ArgumentCaptor<CustomTabTabObserver> mTabObserverCaptor;
    @Mock
    private Tab mTab;
    @Mock
    public TrustedWebActivityUmaRecorder mUmaRecorder;
    @Mock
    private QualityEnforcer.Natives mNativeMock;

    private ShadowPackageManager mShadowPackageManager;

    private QualityEnforcer mQualityEnforcer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(QualityEnforcerJni.TEST_HOOKS, mNativeMock);

        doNothing()
                .when(mTabObserverRegistrar)
                .registerActivityTabObserver(mTabObserverCaptor.capture());

        when(mVerifier.verify(TRUSTED_ORIGIN_PAGE.getSpec())).thenReturn(Promise.fulfilled(true));
        when(mVerifier.verify(UNTRUSTED_PAGE.getSpec())).thenReturn(Promise.fulfilled(false));

        mQualityEnforcer = new QualityEnforcer(mActivity, mLifecycleDispatcher,
                mTabObserverRegistrar, mIntentDataProvider, mCustomTabsConnection, mVerifier,
                mClientPackageNameProvider, mUmaRecorder);
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(TRUSTED_ORIGIN_PAGE.getSpec());
    }

    @Test
    public void trigger_navigateTo404() {
        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verifyTriggered404();
    }

    @Test
    public void notTrigger_navigationSuccess() {
        navigateToUrlNoError(TRUSTED_ORIGIN_PAGE);
        verifyNotTriggered();
    }

    @Test
    public void notTrigger_navigateTo404NotVerifiedSite() {
        navigateToUrlNotFound(UNTRUSTED_PAGE);
        verifyNotTriggered();
    }

    @Test
    public void notTrigger_navigateFromNotVerifiedToVerified404() {
        navigateToUrlNoError(UNTRUSTED_PAGE);
        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verifyNotTriggered();
    }

    @Test
    public void trigger_notVerifiedToVerifiedThen404() {
        navigateToUrlNoError(UNTRUSTED_PAGE);
        navigateToUrlNoError(TRUSTED_ORIGIN_PAGE);
        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verifyTriggered404();
    }

    @Test
    public void trigger_offline() {
        navigateToUrlInternet(TRUSTED_ORIGIN_PAGE);
        verifyToastShown(ContextUtils.getApplicationContext().getString(
                R.string.twa_quality_enforcement_violation_offline, TRUSTED_ORIGIN_PAGE.getSpec()));
        verifyNotifyClientApp();
    }

    @Test
    public void triggerCrash_whenClientSupports() {
        setClientEnable(true);
        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verify(mActivity).finish();
    }

    @Test
    public void notTriggerCrash_whenClientNotSupport() {
        setClientEnable(false);

        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verify(mActivity, never()).finish();
    }

    @Test
    public void notTrigger_digitalAssetLinkPass() {
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(TRUSTED_ORIGIN_PAGE.getSpec());
        navigateToUrlNoError(TRUSTED_ORIGIN_PAGE);
        verifyNotTriggered();
    }

    @Test
    public void trigger_digitalAssetLinkFailed() {
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(UNTRUSTED_PAGE.getSpec());
        navigateToUrlNoError(UNTRUSTED_PAGE);
        verifyToastShown(ContextUtils.getApplicationContext().getString(
                R.string.twa_quality_enforcement_violation_asset_link, UNTRUSTED_PAGE.getSpec()));
        verifyNotifyClientApp();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED)
    public void notTriggerCrash_whenClientNotSupportButForced() {
        setClientEnable(false);

        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verify(mActivity).finish();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)
    public void notTriggerCrash_whenFlagIsDisabled() {
        navigateToUrlNotFound(TRUSTED_ORIGIN_PAGE);
        verifyNotTriggered();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED)
    public void triggerNotCrash_whenDigitalAssetLinkFailed() {
        setClientEnable(true);
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(UNTRUSTED_PAGE.getSpec());
        navigateToUrlNoError(UNTRUSTED_PAGE);
        verifyNotifyClientApp();
        verify(mActivity, never()).finish();
    }

    private void setClientEnable(boolean enabled) {
        Bundle result = new Bundle();
        result.putBoolean(QualityEnforcer.KEY_SUCCESS, enabled);
        when(mCustomTabsConnection.sendExtraCallbackWithResult(
                     any(), eq(QualityEnforcer.CRASH), any()))
                .thenReturn(result);
    }

    private void verifyTriggered404() {
        verifyToastShown(ContextUtils.getApplicationContext().getString(
                R.string.twa_quality_enforcement_violation_error, HTTP_ERROR_NOT_FOUND,
                TRUSTED_ORIGIN_PAGE.getSpec()));
        verifyNotifyClientApp();
    }

    private void verifyNotifyClientApp() {
        verify(mCustomTabsConnection)
                .sendExtraCallbackWithResult(any(), eq(QualityEnforcer.CRASH), any());
    }

    private void verifyNotTriggered() {
        verify(mCustomTabsConnection, never())
                .sendExtraCallbackWithResult(any(), eq(QualityEnforcer.CRASH), any());
        verify(mActivity, never()).finish();
    }

    private void navigateToUrlNoError(GURL url) {
        navigateToUrl(url, HTTP_STATUS_SUCCESS, NetError.OK);
    }

    private void navigateToUrlNotFound(GURL url) {
        navigateToUrl(url, HTTP_ERROR_NOT_FOUND, NetError.OK);
    }

    private void navigateToUrlInternet(GURL url) {
        navigateToUrl(url, HTTP_STATUS_SUCCESS, NetError.ERR_INTERNET_DISCONNECTED);
    }

    private void navigateToUrl(GURL url, int httpStatusCode, @NetError int errorCode) {
        when(mTab.getOriginalUrl()).thenReturn(url);

        NavigationHandle navigation =
                NavigationHandle.createForTesting(url, false /* isRendererInitiated */,
                        0 /* pageTransition */, false /* hasUserGesture */);
        navigation.didFinish(url, false /* isErrorPage */, true /* hasCommitted */,
                false /* isFragmentNavigation */, false /* isDownload */,
                false /* isValidSearchFormUrl */, 0 /* pageTransition */, errorCode, httpStatusCode,
                false /* isExternalProtocol */);
        for (CustomTabTabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);
        }
    }

    private void verifyToastShown(String message) {
        Assert.assertTrue(ShadowToast.showedCustomToast(message, R.id.toast_text));
    }
}
