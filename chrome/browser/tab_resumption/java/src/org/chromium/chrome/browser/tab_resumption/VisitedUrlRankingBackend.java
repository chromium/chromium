// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.visited_url_ranking.ScoredURLUserAction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** The glue code between Tab resumption module and the native fetch and rank services backend. */
@JNINamespace("tab_resumption::jni")
public class VisitedUrlRankingBackend implements SuggestionBackend {
    private static final boolean sShowHistoryAppChip =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final boolean mFetchHisotryEnabled;
    private long mNativeVisitedUrlRankingBackend;
    private boolean mIsTabStateInitialized;

    @Nullable private Runnable mUpdateObserver;

    VisitedUrlRankingBackend(
            Profile profile, ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mNativeVisitedUrlRankingBackend = VisitedUrlRankingBackendJni.get().init(this, profile);
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mFetchHisotryEnabled =
                TabResumptionModuleUtils.TAB_RESUMPTION_FETCH_HISTORY_BACKEND.getValue();
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
    public void read(Callback<List<SuggestionEntry>> callback) {
        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();

        // Updates mIsTabStateInitialized before calling the backend API. If the TabModel
        // initialization hasn't completed at the startup, the local tab data fetcher will return
        // empty results, and we will handle the matching to a local Tab after the suggestions are
        // received.
        if (!mIsTabStateInitialized && mTabModelSelectorSupplier.hasValue()) {
            mIsTabStateInitialized = mTabModelSelectorSupplier.get().isTabStateInitialized();
        }

        // TODO(b/337858147): handles showing local Tabs if returned from
        // VisitedUrlRankingBackendJni.
        VisitedUrlRankingBackendJni.get()
                .getRankedSuggestions(
                        mNativeVisitedUrlRankingBackend,
                        TabResumptionModuleUtils.getCurrentTimeMs(),
                        mFetchHisotryEnabled,
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

    /** Helper to add new {@link SuggestionEntry} to list. */
    @CalledByNative
    private void addSuggestionEntry(
            int type,
            @NonNull String sourceName,
            @NonNull GURL url,
            @NonNull String title,
            long lastActiveTime,
            int localTabId,
            @NonNull String visitId,
            long requestId,
            String appId,
            String reasonToShowTab,
            boolean needMatchLocalTab,
            @NonNull List<SuggestionEntry> suggestions) {
        SuggestionEntry entry =
                new SuggestionEntry(
                        type,
                        // Sets the flag that this suggestion requires to match a local Tab if it
                        // is shown on the module.
                        sourceName,
                        url,
                        title,
                        lastActiveTime,
                        localTabId,
                        sShowHistoryAppChip ? appId : null,
                        reasonToShowTab,
                        mFetchHisotryEnabled && needMatchLocalTab && !mIsTabStateInitialized);
        if (!visitId.isEmpty()) {
            entry.trainingInfo =
                    new TrainingInfo(mNativeVisitedUrlRankingBackend, visitId, requestId);
        }
        suggestions.add(entry);
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

        void destroy(long nativeVisitedUrlRankingBackend);

        void triggerUpdate(long nativeVisitedUrlRankingBackend);

        void getRankedSuggestions(
                long nativeVisitedUrlRankingBackend,
                long beginTimeMs,
                boolean fetchHistory,
                List<SuggestionEntry> suggestions,
                Callback<List<SuggestionEntry>> callback);

        void recordAction(
                long nativeVisitedUrlRankingBackend,
                @ScoredURLUserAction int scoredUrlUserAction,
                String visitId,
                long visitRequestId);
    }
}
