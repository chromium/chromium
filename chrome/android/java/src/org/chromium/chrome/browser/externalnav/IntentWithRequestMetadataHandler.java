// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;

import java.security.SecureRandom;
import java.util.Arrays;

/**
 * This class generates a token for the most recently launched external intent that has
 * any metadata that the browser is interested in. If the external intent resolves
 * to the browser itself, the token will be used to validate the intent and return the
 * store metadata.
 * Since there could be at most one intent chooser at a time, this class only stores
 * the metadata associated with the most recently launched intent. Token for a previously
 * launched intent will be invalidated if a new one comes.
 */
public class IntentWithRequestMetadataHandler {
    /** Extra to record the token associated with the URL request metadata. */
    public static final String EXTRA_REQUEST_METADATA_TOKEN =
            "org.chromium.chrome.browser.request_metadata_token";

    private static final Object INSTANCE_LOCK = new Object();
    private static IntentWithRequestMetadataHandler sIntentWithRequestMetadataHandler;
    private SecureRandom mSecureRandom = new SecureRandom();
    private RequestMetadata mRequestMetadata;
    private byte[] mIntentToken;
    private String mUri;

    /** Class representing the URL request metadata that can be retrieved later. */
    public static class RequestMetadata {
        private final boolean mHasUserGesture;
        private final boolean mIsRendererIntiated;

        public RequestMetadata(boolean hasUserGesture, boolean isRendererIntiated) {
            mHasUserGesture = hasUserGesture;
            mIsRendererIntiated = isRendererIntiated;
        }

        public boolean isRendererInitiated() {
            return mIsRendererIntiated;
        }

        public boolean hasUserGesture() {
            return mHasUserGesture;
        }
    }

    /** Get the singleton instance of this object. */
    public static IntentWithRequestMetadataHandler getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sIntentWithRequestMetadataHandler == null) {
                sIntentWithRequestMetadataHandler = new IntentWithRequestMetadataHandler();
            }
        }
        return sIntentWithRequestMetadataHandler;
    }

    /**
     * Generate a new token for the intent and put the token and request metadata in the
     * intent extra. This will invalidate the token on the previously launched intent with request
     * metadata.
     *
     * @param intent Intent with request metadata.
     * @param metadata Request metadata to be put into the intent extra.
     */
    public void onNewIntentWithRequestMetadata(Intent intent, RequestMetadata metadata) {
        mIntentToken = new byte[32];
        mSecureRandom.nextBytes(mIntentToken);
        intent.putExtra(EXTRA_REQUEST_METADATA_TOKEN, mIntentToken);
        mRequestMetadata = metadata;
        mUri = IntentHandler.getUrlFromIntent(intent);
    }

    /**
     * Get the request metadata from the intent and clear the stored metadata.
     *
     * @param intent Intent that is used to launch chrome.
     * @return Request metadata from the intent if available, or null otherwise.
     */
    public @Nullable RequestMetadata getRequestMetadataAndClear(Intent intent) {
        if (mIntentToken == null || mUri == null) return null;
        byte[] bytes = IntentUtils.safeGetByteArrayExtra(intent, EXTRA_REQUEST_METADATA_TOKEN);
        RequestMetadata result = null;
        if ((bytes != null)
                && Arrays.equals(bytes, mIntentToken)
                && mUri.equals(IntentHandler.getUrlFromIntent(intent))) {
            result = mRequestMetadata;
        }
        clear();
        return result;
    }

    /** Clear the stored metadata. */
    public void clear() {
        mIntentToken = null;
        mUri = null;
        mRequestMetadata = null;
    }
}
