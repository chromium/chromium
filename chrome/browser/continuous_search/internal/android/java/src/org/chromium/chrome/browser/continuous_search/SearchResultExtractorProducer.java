// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.continuous_search.SearchResultExtractorClientStatus;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Extracts search results by parsing in the renderer.
 */
@JNINamespace("continuous_search")
public class SearchResultExtractorProducer extends SearchResultProducer {
    private static final String MINIMUM_URL_COUNT_PARAM = "minimum_url_count";
    private static final int DEFAULT_MINIMUM_URL_COUNT = 5;

    private static final String USE_PROVIDER_ICON_PARAM = "use_provider_icon";
    private static final boolean USE_PROVIDER_ICON_DEFAULT_VALUE = true;

    @VisibleForTesting
    static final @DrawableRes int PROVIDER_ICON_RESOURCE = R.drawable.ic_logo_googleg_20dp;

    private long mNativeSearchResultExtractorProducer;
    private @State int mState;

    @VisibleForTesting
    int mMinimumUrlCount;
    @VisibleForTesting
    boolean mUseProviderIcon;

    @IntDef({State.READY, State.CAPTURING, State.CANCELLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int READY = 0;
        int CAPTURING = 1;
        int CANCELLED = 2;
    };

    SearchResultExtractorProducer(Tab tab, SearchResultListener listener) {
        super(tab, listener);
        mNativeSearchResultExtractorProducer = SearchResultExtractorProducerJni.get().create(this);
        mState = State.READY;
        mMinimumUrlCount = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTINUOUS_SEARCH, MINIMUM_URL_COUNT_PARAM,
                DEFAULT_MINIMUM_URL_COUNT);
        mUseProviderIcon = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTINUOUS_SEARCH, USE_PROVIDER_ICON_PARAM,
                USE_PROVIDER_ICON_DEFAULT_VALUE);
    }

    @CalledByNative
    void onError(int statusCode) {
        if (mState == State.CANCELLED) return;

        assert mState == State.CAPTURING;
        mListener.onError(statusCode);
        mState = State.READY;
    }

    /**
     * Called when results are returned from native and forwards the result to {@link mListener}.
     * This can succeed even after a web contents or tab is destroyed. Ensure {@link #cancel()} is
     * called on this producer when the tab or web contents is destroyed if this behavior is not
     * desired.
     * @param url The URL of SRP data was fetched for.
     * @param query The query associated with the SRP.
     * @param resultCategory The type of results: news, organic, etc.
     * @param groupType One entry per group (g) specifying the type of results.
     * @param groupSize One entry per group (g) specifying the number (n_g) of titles and urls in
     *     the respective group.
     * @param titles One title per item ordered by group. There will be (sum n_g forall g) entries.
     * @param urls One URL per item ordered by group. There will be (sum n_g forall g) entries.
     */
    @CalledByNative
    void onResultsAvailable(GURL url, String query, int resultCategory, int[] groupType,
            int[] groupSize, String[] titles, GURL[] urls) {
        final int oldState = mState;
        mState = State.READY;
        if (oldState == State.CANCELLED) return;

        TraceEvent.begin("SearchResultExtractorProducer#onResultsAvailable");
        int groupOffset = 0;
        int urlCount = 0;
        List<PageGroup> groups = new ArrayList<PageGroup>();
        for (int i = 0; i < groupType.length; i++) {
            List<PageItem> results = new ArrayList<PageItem>();
            Set<GURL> groupUrls = new HashSet<>();
            for (int j = 0; j < groupSize[i]; j++) {
                // Uniquify urls within the group.
                if (!groupUrls.add(urls[groupOffset + j])) continue;

                results.add(new PageItem(urls[groupOffset + j], titles[groupOffset + j]));
                urlCount++;
            }

            groupOffset += groupSize[i];
            groups.add(new PageGroup(/*label=*/"", false, results));
        }

        if (urlCount < mMinimumUrlCount) {
            mListener.onError(SearchResultExtractorClientStatus.NOT_ENOUGH_RESULTS);
            TraceEvent.end("SearchResultExtractorProducer#onResultsAvailable");
            return;
        }

        assert !GURL.isEmptyOrInvalid(url);
        assert query != null && !query.isEmpty();
        ContinuousNavigationMetadata.Provider provider = new ContinuousNavigationMetadata.Provider(
                resultCategory, getProviderName(resultCategory),
                mUseProviderIcon ? PROVIDER_ICON_RESOURCE : 0);
        ContinuousNavigationMetadata metadata =
                new ContinuousNavigationMetadata(url, query, provider, groups);
        mListener.onResult(metadata);
        TraceEvent.end("SearchResultExtractorProducer#onResultsAvailable");
    }

    @Override
    void fetchResults(GURL url, String query) {
        if (mState != State.READY) {
            mListener.onError(SearchResultExtractorClientStatus.ALREADY_CAPTURING);
            return;
        }
        if (mNativeSearchResultExtractorProducer == 0) {
            mListener.onError(SearchResultExtractorClientStatus.NATIVE_NOT_INITIALIZED);
            return;
        }

        WebContents contents = mTab.getWebContents();
        if (contents == null) {
            mListener.onError(SearchResultExtractorClientStatus.WEB_CONTENTS_GONE);
            return;
        }
        if (!url.equals(contents.getLastCommittedUrl())) {
            mListener.onError(SearchResultExtractorClientStatus.UNEXPECTED_URL);
            return;
        }

        TraceEvent.begin("SearchResultExtractorProducer#fetchResults");
        mState = State.CAPTURING;
        SearchResultExtractorProducerJni.get().fetchResults(
                mNativeSearchResultExtractorProducer, mTab.getWebContents(), query);
        TraceEvent.end("SearchResultExtractorProducer#fetchResults");
    }

    @Override
    void cancel() {
        if (mState == State.CAPTURING) {
            mState = State.CANCELLED;
        }
    }

    @Override
    int getSuccessStatus() {
        return SearchResultExtractorClientStatus.SUCCESS;
    }

    void destroy() {
        if (mNativeSearchResultExtractorProducer == 0) return;

        SearchResultExtractorProducerJni.get().destroy(mNativeSearchResultExtractorProducer);
        mNativeSearchResultExtractorProducer = 0;
    }

    private String getProviderName(int resultCategory) {
        if (mUseProviderIcon) return null;

        // (TODO:crbug/1199339) Replace hardcoded string with translated resources.
        switch (resultCategory) {
            case PageCategory.ORGANIC_SRP:
            case PageCategory.NEWS_SRP:
                return "Google Search";
            default:
                assert false : "Invalid result category: " + resultCategory;
                return null;
        }
    }

    @NativeMethods
    interface Natives {
        long create(SearchResultExtractorProducer producer);
        void fetchResults(
                long nativeSearchResultExtractorProducer, WebContents webContents, String query);
        void destroy(long nativeSearchResultExtractorProducer);
    }
}
