// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;


import android.content.Context;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchResponse;

/**
 * Interface to interact with a system AI assistant, used to invoke a UI to ask questions or
 * summarize a web page.
 */
@NullMarked
public abstract class SystemAiProvider {

    /**
     * Checks if the AI is enabled, available and which capabilities are supported.
     *
     * @param context Android context.
     * @param request A message with the capabilities we'd like to use.
     * @return A promise with a message indicating if the AI is available, and which capabilities
     *     are supported.
     */
    public abstract ListenableFuture<AvailabilityResponse> isAvailable(
            Context context, AvailabilityRequest request);

    /**
     * Invokes the assistant UI for a specific capability.
     *
     * @param context Android context.
     * @param request A message indicating which capability we'd like to launch, and additional
     *     context.
     * @return A promise with a message (currently empty).
     */
    public abstract ListenableFuture<LaunchResponse> launch(Context context, LaunchRequest request);
}
