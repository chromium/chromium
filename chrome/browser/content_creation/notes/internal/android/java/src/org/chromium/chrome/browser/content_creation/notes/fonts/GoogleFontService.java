// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.fonts;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import androidx.core.provider.FontRequest;
import androidx.core.provider.FontsContractCompat;

import org.chromium.chrome.browser.content_creation.internal.R;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Service in charge of using Android's Downloadable Fonts APIs to load Google
 * Fonts Typeface instances.
 */
public class GoogleFontService {
    private static final String THREAD_NAME = "GoogleFontHandlerThread";

    private final Activity mActivity;

    public GoogleFontService(Activity activity) {
        this.mActivity = activity;
    }

    /**
     * Fetches Google Fonts Typeface instances based on the provided set of |requests|.
     * Asynchronously returns the responses as a Map from the original requests to
     * their responses via the |resultsCallback|.
     */
    public void fetchFonts(
            Set<TypefaceRequest> requests, GoogleFontRequestCallback resultsCallback) {
        if (requests == null || requests.isEmpty() || resultsCallback == null) {
            return;
        }

        HandlerThread handlerThread = new HandlerThread(THREAD_NAME);
        handlerThread.start();

        Map<TypefaceRequest, TypefaceResponse> results = new HashMap<>();

        for (TypefaceRequest request : requests) {
            FontRequest fontRequest =
                    new FontRequest(
                            "com.google.android.gms.fonts",
                            "com.google.android.gms",
                            request.toQuery(),
                            R.array.ui_com_google_android_gms_fonts_certs);

            FontsContractCompat.FontRequestCallback fetchingCallback =
                    new FontsContractCompat.FontRequestCallback() {
                        @Override
                        public void onTypefaceRetrieved(Typeface typeface) {
                            results.put(request, new TypefaceResponse(typeface));
                            onResultsUpdated(
                                    results, requests.size(), resultsCallback, handlerThread);
                        }

                        @Override
                        public void onTypefaceRequestFailed(int reason) {
                            results.put(request, new TypefaceResponse(reason));
                            onResultsUpdated(
                                    results, requests.size(), resultsCallback, handlerThread);
                        }
                    };

            Handler handler = new Handler(handlerThread.getLooper());
            FontsContractCompat.requestFont(this.mActivity, fontRequest, fetchingCallback, handler);
        }
    }

    private void onResultsUpdated(
            Map<TypefaceRequest, TypefaceResponse> resultsMap,
            int nbExpectedResults,
            GoogleFontRequestCallback callback,
            HandlerThread handlerThread) {
        if (resultsMap.size() != nbExpectedResults) {
            // Still missing results, wait for more to come in.
            return;
        }

        // Make sure the results are returned on the main thread, and exit the
        // worker thread.
        new Handler(Looper.getMainLooper())
                .post(
                        () -> {
                            callback.onResponsesReceived(resultsMap);
                        });
        handlerThread.quitSafely();
    }

    /**
     * Callback type for asynchronously receiving the results of fetching
     * Google Fonts typefaces.
     */
    public static class GoogleFontRequestCallback {
        public void onResponsesReceived(Map<TypefaceRequest, TypefaceResponse> resultsMap) {
            // No-op.
        }
    }
}
