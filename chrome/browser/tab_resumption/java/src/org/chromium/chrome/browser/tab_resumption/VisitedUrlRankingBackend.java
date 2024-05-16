// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** The glue code between Tab resumption module and the native fetch and rank services backend. */
@JNINamespace("tab_resumption::jni")
public class VisitedUrlRankingBackend implements SuggestionBackend {
    private long mNativeVisitedUrlRankingBackend;

    @Nullable private Runnable mUpdateObserver;

    VisitedUrlRankingBackend(Profile profile) {
        mNativeVisitedUrlRankingBackend = VisitedUrlRankingBackendJni.get().init(this, profile);
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void destroy() {
        mUpdateObserver = null;
        VisitedUrlRankingBackendJni.get().destroy(mNativeVisitedUrlRankingBackend);
        mNativeVisitedUrlRankingBackend = 0;
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void triggerUpdate() {
        VisitedUrlRankingBackendJni.get().triggerUpdate(mNativeVisitedUrlRankingBackend);
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void setUpdateObserver(Runnable updateObserver) {
        mUpdateObserver = updateObserver;
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void readCached(Callback<List<SuggestionEntry>> callback) {
        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();

        VisitedUrlRankingBackendJni.get()
                .getRankedSuggestions(
                        mNativeVisitedUrlRankingBackend,
                        TabResumptionModuleUtils.getCurrentTimeMs(),
                        suggestions,
                        callback);
    }

    /** Member method callback to trigger update. */
    @CalledByNative
    private void onRefresh() {
        if (mUpdateObserver != null) {
            mUpdateObserver.run();
        }
    }

    /** Helper to add new SuggestionEntry to list. */
    @CalledByNative
    private static void addSuggestionEntry(
            String sourceName,
            GURL url,
            String title,
            long lastActiveTime,
            int id,
            List<SuggestionEntry> suggestions) {
        // TODO(crbug.com/337858147): Handle Local Tab suggestions case.
        suggestions.add(new SuggestionEntry(sourceName, url, title, lastActiveTime, id));
    }

    /** Helper to call previously injected callback to pass suggestion results. */
    @CalledByNative
    private static void onSuggestions(
            List<SuggestionEntry> suggestions, Callback<List<SuggestionEntry>> callback) {
        callback.onResult(suggestions);
    }

    @NativeMethods
    interface Natives {
        long init(VisitedUrlRankingBackend self, @JniType("Profile*") Profile profile);

        void triggerUpdate(long nativeVisitedUrlRankingBackend);

        void getRankedSuggestions(
                long nativeVisitedUrlRankingBackend,
                long beginTimeMs,
                List<SuggestionEntry> suggestions,
                Callback<List<SuggestionEntry>> callback);

        void destroy(long nativeVisitedUrlRankingBackend);
    }
}
