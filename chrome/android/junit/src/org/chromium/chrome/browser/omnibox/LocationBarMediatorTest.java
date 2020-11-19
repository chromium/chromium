// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static junit.framework.Assert.assertNull;

import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;

/** Unit tests for LocationBarMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
public class LocationBarMediatorTest {
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

    private LocationBarMediator mMediator;

    @Before
    public void setUp() {
        doReturn(mContext).when(mLocationBarLayout).getContext();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        mMediator = new LocationBarMediator(
                mLocationBarLayout, mLocationBarDataProvider, mAssistantVoiceSearchSupplier);
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
}
