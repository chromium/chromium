// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import com.google.common.util.concurrent.Futures;
import com.google.protobuf.InvalidProtocolBufferException;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.Capability;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceAvailable;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceNotAvailable;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.content_extraction.InnerTextBridgeJni;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

/** Unit tests for {@link AiAssistantService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AiAssistantServiceUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private WebContents mWebContents;
    @Mock private Tab mTab;
    @Mock SystemAiProvider mSystemAiProvider;
    @Mock SystemAiProviderFactory mSystemAiProviderFactory;
    @Mock private InnerTextBridge.Natives mInnerTextNatives;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;
    @Captor private ArgumentCaptor<LaunchRequest> mLaunchRequestCaptor;

    @Before
    public void setUp() throws Exception {
        when(mSystemAiProviderFactory.createSystemAiProvider()).thenReturn(mSystemAiProvider);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);

        InnerTextBridgeJni.setInstanceForTesting(mInnerTextNatives);
    }

    @Test
    public void showAi_fallsBackToIntentWhenNoDownstreamImpl() {
        ServiceLoaderUtil.setInstanceForTesting(SystemAiProvider.class, null);
        var pageContents = "Page contents for one.com";
        setInnerTextExtractionResult(pageContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        var service = new AiAssistantService();

        service.showAi(mContext, mTab);
        ShadowLooper.idleMainLooper();

        var launchRequest = assertVoiceActivityStartedWithLaunchRequest();
        assertLaunchRequestIsForSummarizeUrl(
                launchRequest, JUnitTestGURLs.URL_1.getSpec(), pageContents);
    }

    @Test
    public void showAi_fallsBackToIntentWhenDownstreamImplNotAvailable() {
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        var pageContents = "Page contents for google.com/dog";
        setInnerTextExtractionResult(pageContents);
        setSystemAiProviderAsUnavailable();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_DOG);

        var service = new AiAssistantService();
        service.showAi(mContext, mTab);
        ShadowLooper.idleMainLooper();

        var launchRequest = assertVoiceActivityStartedWithLaunchRequest();
        assertLaunchRequestIsForSummarizeUrl(
                launchRequest, JUnitTestGURLs.GOOGLE_URL_DOG.getSpec(), pageContents);
    }

    @Test
    public void showAi_fallsBackToIntentWhenDownstreamImplNotAvailable_pdfPage() {
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsUnavailable();
        var pdfUri = JUnitTestGURLs.URL_1_WITH_PDF_PATH.getSpec();
        var pdfPage = mock(PdfPage.class);
        when(pdfPage.getUri()).thenReturn(Uri.parse(pdfUri));
        when(pdfPage.getTitle()).thenReturn("file.pdf");
        when(mTab.getNativePage()).thenReturn(pdfPage);

        var service = new AiAssistantService();
        service.showAi(mContext, mTab);

        var launchRequest = assertVoiceActivityStartedWithLaunchRequest();
        assertLaunchRequestIsForAnalyzeAttachment(launchRequest, pdfUri);
    }

    @Test
    public void showAi_usesDownstreamImplWhenAvailable() {
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        setSystemAiProviderAsAvailable();
        var pageContents = "Page contents for URL_2";
        setInnerTextExtractionResult(pageContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        var service = new AiAssistantService();
        service.showAi(mContext, mTab);
        ShadowLooper.idleMainLooper();

        verify(mSystemAiProvider).launch(eq(mContext), mLaunchRequestCaptor.capture());
        assertLaunchRequestIsForSummarizeUrl(
                mLaunchRequestCaptor.getValue(), JUnitTestGURLs.URL_2.getSpec(), pageContents);
    }

    @Test
    public void showAi_usesAnalyzeDocumentForPdfs() {
        ServiceLoaderUtil.setInstanceForTesting(
                SystemAiProviderFactory.class, mSystemAiProviderFactory);
        var pdfUri = "https://google.com/file.pdf";
        var pdfPage = mock(PdfPage.class);
        when(pdfPage.getUri()).thenReturn(Uri.parse(pdfUri));
        when(pdfPage.getTitle()).thenReturn("file.pdf");
        when(mTab.getNativePage()).thenReturn(pdfPage);
        setSystemAiProviderAsAvailable();

        var service = new AiAssistantService();

        service.showAi(mContext, mTab);

        verify(mSystemAiProvider).launch(eq(mContext), mLaunchRequestCaptor.capture());
        assertLaunchRequestIsForAnalyzeAttachment(mLaunchRequestCaptor.getValue(), pdfUri);
    }

    private void setSystemAiProviderAsAvailable() {
        var serviceAvailableBuilder =
                ServiceAvailable.newBuilder()
                        .addSupportedCapabilities(Capability.ANALYZE_ATTACHMENT_CAPABILITY)
                        .addSupportedCapabilities(Capability.SUMMARIZE_URL_CAPABILITY);
        var availabilityResponse =
                AvailabilityResponse.newBuilder().setAvailable(serviceAvailableBuilder).build();
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

    private void setInnerTextExtractionResult(String result) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> callback =
                                    (Callback<Optional<String>>)
                                            invocationOnMock.getArgument(1, Callback.class);
                            callback.onResult(Optional.of(result));
                            return null;
                        })
                .when(mInnerTextNatives)
                .getInnerText(eq(mRenderFrameHost), any());
    }

    private void assertLaunchRequestIsForSummarizeUrl(
            LaunchRequest launchRequest, String expectedUrl, String expectedPageContents) {
        Assert.assertTrue(launchRequest.hasSummarizeUrl());
        Assert.assertTrue(launchRequest.getSummarizeUrl().hasUrlContext());
        Assert.assertEquals(expectedUrl, launchRequest.getSummarizeUrl().getUrlContext().getUrl());
        Assert.assertEquals(
                expectedPageContents,
                launchRequest.getSummarizeUrl().getUrlContext().getPageContent());
    }

    private void assertLaunchRequestIsForAnalyzeAttachment(
            LaunchRequest launchRequest, String expectedUri) {
        Assert.assertTrue(launchRequest.hasAnalyzeAttachment());
        assertThat(launchRequest.getAnalyzeAttachment().getFilesCount(), greaterThanOrEqualTo(1));
        Assert.assertEquals(expectedUri, launchRequest.getAnalyzeAttachment().getFiles(0).getUri());
    }

    private LaunchRequest assertVoiceActivityStartedWithLaunchRequest() {
        verify(mContext).startActivity(mIntentCaptor.capture());

        var startedIntent = mIntentCaptor.getValue();
        var launchRequestBytes =
                startedIntent.getByteArrayExtra(AiAssistantService.EXTRA_LAUNCH_REQUEST);
        Assert.assertEquals(Intent.ACTION_VOICE_COMMAND, startedIntent.getAction());

        try {
            return LaunchRequest.parseFrom(launchRequestBytes);
        } catch (InvalidProtocolBufferException ex) {
            Assert.fail("Exception while parsing proto from intent");
            return null;
        }
    }
}
