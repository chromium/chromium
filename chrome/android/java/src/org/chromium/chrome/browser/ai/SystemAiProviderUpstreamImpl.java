// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import android.content.Context;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ServiceNotAvailable;

/**
 * Instantiable version of {@link SystemAiProvider}, don't add anything to this class. Downstream
 * provides an actual implementation via ServiceLoader/@ServiceImpl.
 */
@NullMarked
class SystemAiProviderUpstreamImpl extends SystemAiProvider {

    @Override
    public ListenableFuture<AvailabilityResponse> isAvailable(
            Context context, AvailabilityRequest request) {
        var response =
                AvailabilityResponse.newBuilder()
                        .setNotAvailable(ServiceNotAvailable.getDefaultInstance())
                        .build();

        return Futures.immediateFuture(response);
    }

    @Override
    public ListenableFuture<LaunchResponse> launch(Context context, LaunchRequest request) {
        return Futures.immediateFuture(LaunchResponse.getDefaultInstance());
    }
}
