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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.SummarizeUrl;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.UrlContext;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;

/**
 * Service to interact with an AI assistant, used to invoke an assistant UI to summarize web pages
 * and/or ask questions about it. System-specific integration is handled with the SystemAiProvider
 * interface, and if none is present it falls back to sending an ACTION_VOICE_COMMAND intent to
 * invoke the voice assistant UI.
 */
@NullMarked
public class AiAssistantService {

    private static final String TAG = "AiAssistantService";

    private final SystemAiProvider mSystemAiProvider;

    /** Creates an instance of AiAssistantService. */
    public AiAssistantService() {
        var provider = ServiceLoaderUtil.maybeCreate(SystemAiProvider.class);
        mSystemAiProvider = provider != null ? provider : new SystemAiProviderUpstreamImpl();
    }

    /**
     * Shows an AI assistant UI for the current tab.
     *
     * @param context Current activity.
     * @param tab Current tab.
     */
    public void showAi(Context context, Tab tab) {
        if (!isTabElegible(tab)) return;

        var availabilityRequest = AvailabilityRequest.getDefaultInstance();
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
                        showAssistant(context);
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

        if (result != null && result.hasAvailable()) {
            var urlContext = UrlContext.newBuilder().setUrl(tab.getUrl().getSpec()).build();
            var summarizeUrl = SummarizeUrl.newBuilder().setUrlContext(urlContext).build();
            var launchRequest = LaunchRequest.newBuilder().setSummarizeUrl(summarizeUrl).build();

            mSystemAiProvider.launch(context, launchRequest);
        } else {
            showAssistant(context);
        }
    }

    private void showAssistant(Context context) {
        Log.w(TAG, "Unable to use to system AI provider, sending intent instead");
        Intent assistantIntent = new Intent(Intent.ACTION_VOICE_COMMAND);
        try {
            context.startActivity(assistantIntent);
        } catch (ActivityNotFoundException ex) {
            // Ignore exception, as this is a fallback.
            Log.w(TAG, "Exception while trying to send ACTION_VOICE_COMMAND intent", ex);
        }
    }
}
