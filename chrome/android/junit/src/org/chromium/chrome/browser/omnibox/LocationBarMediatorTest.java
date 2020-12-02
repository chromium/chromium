// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNull;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.BuildConfig;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/** Unit tests for LocationBarMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
public class LocationBarMediatorTest {
    private static final String TEST_URL = "http://testurl.com";

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    LocationBarLayout mLocationBarLayout;
    @Mock
    Context mContext;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private LocationBarDataProvider mLocationBarDataProvider;
    @Mock
    private OneshotSupplierImpl<AssistantVoiceSearchService> mAssistantVoiceSearchSupplier;
    @Mock
    private OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    @Mock
    private LocaleManager mLocaleManager;
    @Mock
    private Profile.Natives mProfileNativesJniMock;
    @Mock
    private Tab mTab;
    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private LocationBarMediator mMediator;

    @Before
    public void setUp() {
        doReturn(mContext).when(mLocationBarLayout).getContext();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileNativesJniMock);
        mMediator = new LocationBarMediator(mLocationBarLayout, mLocationBarDataProvider,
                mAssistantVoiceSearchSupplier, mOverrideUrlLoadingDelegate, mLocaleManager);
    }

    @Test
    public void testVoiceSearchService_initializedWithNative() {
        mMediator.onFinishNativeInitialization();
        verify(mAssistantVoiceSearchSupplier).set(notNull());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testVoiceSearchService_initializedWithNative_featureDisabled() {
        mMediator.onFinishNativeInitialization();
        verify(mAssistantVoiceSearchSupplier).set(notNull());
    }

    @Test
    public void testGetVoiceRecognitionHandler_safeToCallAfterDestroy() {
        mMediator.onFinishNativeInitialization();
        mMediator.destroy();
        assertNull(mMediator.getVoiceRecognitionHandler());
    }

    @Test
    public void testOnTabLoadingNtp() {
        mMediator.onNtpStartedLoading();
        verify(mLocationBarLayout).onNtpStartedLoading();
    }

    @Test
    public void testLoadUrl() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mMediator.loadUrl(TEST_URL, PageTransition.TYPED, 0);

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    @Test
    public void testLoadUrl_NativeNotInitialized() {
        if (BuildConfig.DCHECK_IS_ON) {
            // clang-format off
            try {
                mMediator.loadUrl(TEST_URL, PageTransition.TYPED, 0);
                throw new Error("Expected an assert to be triggered.");
            } catch (AssertionError e) {}
            // clang-format on
        }
    }

    @Test
    public void testLoadUrl_OverrideLoadingDelegate() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(TEST_URL, PageTransition.TYPED, null, null, false);
        mMediator.loadUrl(TEST_URL, PageTransition.TYPED, 0);

        verify(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(TEST_URL, PageTransition.TYPED, null, null, false);
        verify(mTab, times(0)).loadUrl(any());
    }
}
