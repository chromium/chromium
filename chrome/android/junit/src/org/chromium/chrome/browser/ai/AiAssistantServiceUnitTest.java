// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;

import com.google.common.util.concurrent.Futures;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceAvailable;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceNotAvailable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link AiAssistantService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AiAssistantServiceUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private Tab mTab;
    @Mock SystemAiProvider mSystemAiProvider;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    @Before
    public void setUp() throws Exception {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);
    }

    @Test
    public void showAi_fallsBackToIntentWhenNoDownstreamImpl() {
        ServiceLoaderUtil.setInstanceForTesting(SystemAiProvider.class, null);

        var service = new AiAssistantService();

        service.showAi(mContext, mTab);

        assertVoiceActivityStarted();
    }

    @Test
    public void showAi_fallsBackToIntentWhenDownstreamImplNotAvailable() {
        ServiceLoaderUtil.setInstanceForTesting(SystemAiProvider.class, mSystemAiProvider);
        setSystemAiProviderAsUnavailable();

        var service = new AiAssistantService();
        service.showAi(mContext, mTab);

        assertVoiceActivityStarted();
    }

    @Test
    public void showAi_usesDownstreamImplWhenAvailable() {
        ServiceLoaderUtil.setInstanceForTesting(SystemAiProvider.class, mSystemAiProvider);
        setSystemAiProviderAsAvailable();

        var service = new AiAssistantService();

        service.showAi(mContext, mTab);

        verify(mSystemAiProvider).launch(eq(mContext), any());
    }

    private void setSystemAiProviderAsAvailable() {
        var availabilityResponse =
                AvailabilityResponse.newBuilder()
                        .setAvailable(ServiceAvailable.getDefaultInstance())
                        .build();
        when(mSystemAiProvider.isAvailable(eq(mContext), any()))
                .thenReturn(Futures.immediateFuture(availabilityResponse));
    }

    private void setSystemAiProviderAsUnavailable() {
        var availabilityResponse =
                AvailabilityResponse.newBuilder()
                        .setNotAvailable(ServiceNotAvailable.getDefaultInstance())
                        .build();
        when(mSystemAiProvider.isAvailable(eq(mContext), any()))
                .thenReturn(Futures.immediateFuture(availabilityResponse));
    }

    private void assertVoiceActivityStarted() {
        verify(mContext).startActivity(mIntentCaptor.capture());

        var startedIntent = mIntentCaptor.getValue();

        Assert.assertEquals(Intent.ACTION_VOICE_COMMAND, startedIntent.getAction());
    }
}
