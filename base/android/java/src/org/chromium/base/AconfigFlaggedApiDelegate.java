// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ClipDescription;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.webkit.WebViewDelegate;
import android.window.TrustedPresentationThresholds;

import org.chromium.base.hid.HidManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Executor;

/** Interface to call unreleased Android APIs that are guarded by aconfig flags. */
@NullMarked
public interface AconfigFlaggedApiDelegate {
    /**
     * Prefer to use this to get a instance instead of calling ServiceLoaderUtil. If possible, avoid
     * caching the return value in member or global variables as it allows more compile time
     * optimizations.
     */
    static @Nullable AconfigFlaggedApiDelegate getInstance() {
        return ServiceLoaderUtil.maybeCreate(AconfigFlaggedApiDelegate.class);
    }

    static void setInstanceForTesting(AconfigFlaggedApiDelegate testInstance) {
        ServiceLoaderUtil.setInstanceForTesting(AconfigFlaggedApiDelegate.class, testInstance);
    }

    /** Gets an Android HidManager wrapped in an intermediary object. */
    default @Nullable HidManager getHidManager() {
        return null;
    }

    /** Checks whether content restriction is supported and enabled for WebViews. */
    default boolean isContentRestrictionEnabled() {
        return false;
    }

    /**
     * Calls the platform to determine if the content should be allowed or blocked.
     *
     * @param uri The URI of the content to be classified.
     * @param requestBody The request body of the content to be classified. Can be null for requests
     *     that have no body (for ex. GET requests).
     * @param mimeType The MIME type of the content to be classified.
     * @param executor The executor to run the callback on.
     * @return A promise fulfilled with the boolean classification result (true if allowed),
     *     rejected otherwise with {@link UnsupportedOperationException} if not supported or with
     *     the exception received from the API call.
     */
    default Promise<Boolean> requestContentRestrictionClassification(
            Uri uri,
            @Nullable ParcelFileDescriptor requestBody,
            String mimeType,
            Executor executor) {
        Promise<Boolean> promise = new Promise<>();
        promise.reject(new UnsupportedOperationException("Not supported"));
        return promise;
    }

    /**
     * Sends an intent to the Android platform to display a dialog about the restricted content.
     *
     * @param uri The URI of the content being restricted.
     * @return true if the intent was sent successfully, false otherwise.
     */
    default boolean sendShowRestrictedContentIntent(Uri uri) {
        return false;
    }

    /**
     * Checks if the Native WebView Zygote is enabled.
     *
     * @param delegate the WebViewDelegate used to check the state.
     */
    default boolean isNativeWebViewZygoteEnabled(WebViewDelegate delegate) {
        return false;
    }

    /**
     * Creates a {@link android.window.TrustedPresentationThresholds} instance using the upcoming
     * strict occlusion API if supported, otherwise returns {@code null}.
     */
    default @Nullable TrustedPresentationThresholds createTrustedPresentationThresholdsStrictMode(
            float minAlpha, float minFraction, int stabilityRequirementMs) {
        return null;
    }

    /** Returns whether the new strict occlusion API is available. */
    default boolean isStrictOcclusionAvailable() {
        return false;
    }

    /**
     * Returns whether the ClipDescription has a content-URI.
     *
     * @param clipDescription ClipDescription.
     */
    default boolean hasContentUri(ClipDescription clipDescription) {
        return false;
    }
}
