// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.MoreExecutors;

import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AnalyzeAttachment;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.Capability;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.File;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.SummarizeUrl;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.UrlContext;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.Optional;

/**
 * Service to interact with an AI assistant, used to invoke an assistant UI to summarize web pages
 * and/or ask questions about it. System-specific integration is handled with the SystemAiProvider
 * interface, and if none is present it falls back to sending an ACTION_VOICE_COMMAND intent to
 * invoke the voice assistant UI.
 */
@NullMarked
public class AiAssistantService {

    private static final String TAG = "AiAssistantService";
    static final String EXTRA_LAUNCH_REQUEST =
            "org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest";

    private final SystemAiProvider mSystemAiProvider;

    /** Creates an instance of AiAssistantService. */
    public AiAssistantService() {
        var providerFactory = ServiceLoaderUtil.maybeCreate(SystemAiProviderFactory.class);
        if (providerFactory != null) {
            mSystemAiProvider = providerFactory.createSystemAiProvider();
        } else {
            mSystemAiProvider = new SystemAiProviderUpstreamImpl();
        }
    }

    /**
     * Shows an AI assistant UI for the current tab.
     *
     * @param context Current activity.
     * @param tab Current tab.
     */
    public void showAi(Context context, Tab tab) {
        if (!isTabElegible(tab)) return;

        var availabilityRequest =
                AvailabilityRequest.newBuilder()
                        .addRequestedCapabilities(Capability.ANALYZE_ATTACHMENT_CAPABILITY)
                        .addRequestedCapabilities(Capability.SUMMARIZE_URL_CAPABILITY)
                        .build();
        var availabilityFuture = mSystemAiProvider.isAvailable(context, availabilityRequest);

        Futures.addCallback(
                availabilityFuture,
                new FutureCallback<AvailabilityResponse>() {
                    @Override
                    public void onSuccess(@Nullable AvailabilityResponse result) {
                        onAvailabilityResponse(context, tab, result);
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        Log.w(TAG, "Error getting system AI provider availability: ", t);
                        onAvailabilityResponse(context, tab, null);
                    }
                },
                MoreExecutors.directExecutor());
    }

    private boolean isTabElegible(Tab tab) {
        if (tab == null || tab.getUrl() == null || tab.isOffTheRecord()) return false;

        if (tab.getNativePage() instanceof PdfPage) return true;

        return UrlUtilities.isHttpOrHttps(tab.getUrl());
    }

    private void onAvailabilityResponse(
            Context context, Tab tab, @Nullable AvailabilityResponse result) {
        if (!isTabElegible(tab)) return;

        boolean isSystemAiProviderAvailable = result != null && result.hasAvailable();
        final boolean isSummarizeUrlAvailable;
        final boolean isAnalyzeAttachmentAvailable;
        if (isSystemAiProviderAvailable && result != null) {
            isSummarizeUrlAvailable =
                    result.getAvailable()
                            .getSupportedCapabilitiesList()
                            .contains(Capability.SUMMARIZE_URL_CAPABILITY);
            isAnalyzeAttachmentAvailable =
                    result.getAvailable()
                            .getSupportedCapabilitiesList()
                            .contains(Capability.ANALYZE_ATTACHMENT_CAPABILITY);
        } else {
            isSummarizeUrlAvailable = true;
            isAnalyzeAttachmentAvailable = true;
        }

        if (isTabPdf(tab) && tab.getNativePage() instanceof PdfPage pdfPage) {
            sendLaunchRequest(
                    context,
                    getLaunchRequestForAnalyzeAttachment(pdfPage),
                    /* shouldUseSystemProvider= */ isSystemAiProviderAvailable
                            && isAnalyzeAttachmentAvailable);
        } else if (isTabWebPage(tab)) {
            ThreadUtils.postOnUiThread(
                    () -> {
                        if (tab.getWebContents() == null || tab.isDestroyed()) return;
                        var mainFrame = tab.getWebContents().getMainFrame();
                        var shouldUseSystemProvider =
                                isSystemAiProviderAvailable && isSummarizeUrlAvailable;
                        InnerTextBridge.getInnerText(
                                mainFrame,
                                innerText -> {
                                    onInnerTextReceived(
                                            context, tab, shouldUseSystemProvider, innerText);
                                });
                    });
        }
    }

    private void onInnerTextReceived(
            Context context, Tab tab, boolean shouldUseSystemProvider, Optional<String> innerText) {
        if (innerText.isEmpty()) {
            Log.w(TAG, "Error while extracting page contents");
            return;
        }

        sendLaunchRequest(
                context,
                getLaunchRequestForSummarizeUrl(tab, innerText.get()),
                shouldUseSystemProvider);
    }

    private boolean isTabPdf(Tab tab) {
        if (tab.getNativePage() instanceof PdfPage pdfPage) {
            return pdfPage.getUri() != null;
        }
        return false;
    }

    private boolean isTabWebPage(Tab tab) {
        return !isTabPdf(tab)
                && UrlUtilities.isHttpOrHttps(tab.getUrl())
                && tab.getWebContents() != null;
    }

    private LaunchRequest getLaunchRequestForAnalyzeAttachment(PdfPage pdfPage) {
        assert pdfPage.getUri() != null;
        var file =
                File.newBuilder()
                        .setUri(pdfPage.getUri().toString())
                        .setDisplayName(pdfPage.getTitle())
                        .setMimeType("application/pdf");
        var analyzeAttachment = AnalyzeAttachment.newBuilder().addFiles(file);
        var launchRequest =
                LaunchRequest.newBuilder().setAnalyzeAttachment(analyzeAttachment).build();

        return launchRequest;
    }

    private LaunchRequest getLaunchRequestForSummarizeUrl(Tab tab, String innerText) {
        var urlContext =
                UrlContext.newBuilder().setUrl(tab.getUrl().getSpec()).setPageContent(innerText);
        var summarizeUrl = SummarizeUrl.newBuilder().setUrlContext(urlContext);
        var launchRequest = LaunchRequest.newBuilder().setSummarizeUrl(summarizeUrl).build();

        return launchRequest;
    }

    private void sendLaunchRequest(
            Context context, LaunchRequest launchRequest, boolean shouldUseSystemProvider) {
        if (shouldUseSystemProvider) {
            sendLaunchRequestToSystemProvider(context, launchRequest);
        } else {
            sendLaunchRequestWithIntent(context, launchRequest);
        }
    }

    private void sendLaunchRequestToSystemProvider(Context context, LaunchRequest launchRequest) {
        mSystemAiProvider.launch(context, launchRequest);
    }

    private void sendLaunchRequestWithIntent(Context context, LaunchRequest launchRequest) {
        Log.w(TAG, "Unable to use to system AI provider, sending intent instead");
        Intent assistantIntent = new Intent(Intent.ACTION_VOICE_COMMAND);
        assistantIntent.putExtra(EXTRA_LAUNCH_REQUEST, launchRequest.toByteArray());
        try {
            context.startActivity(assistantIntent);
        } catch (ActivityNotFoundException ex) {
            // Ignore exception, as this is a fallback.
            Log.w(TAG, "Exception while trying to send ACTION_VOICE_COMMAND intent", ex);
        }
    }
}
