// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.continuous_search.SearchResultExtractorClientStatus;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Extracts search results by parsing in the renderer.
 */
@JNINamespace("continuous_search")
public class SearchResultExtractorProducer extends SearchResultProducer {
    private long mNativeSearchResultExtractorProducer;
    private @State int mState;

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
    }

    @CalledByNative
    void onError(int statusCode) {
        if (mState == State.CANCELLED) return;

        assert mState == State.CAPTURING;
        mListener.onError(statusCode);
        mState = State.READY;
    }

    @CalledByNative
    void onResultsAvailable(GURL url, String query, int resultCategory, String[] groupLabel,
            boolean[] isAdGroup, int[] groupSize, String[] titles, GURL[] urls) {
        final int oldState = mState;
        mState = State.READY;
        if (oldState == State.CANCELLED) return;

        int groupOffset = 0;
        List<PageGroup> groups = new ArrayList<PageGroup>();
        for (int i = 0; i < groupLabel.length; i++) {
            List<PageItem> results = new ArrayList<PageItem>();
            for (int j = 0; j < groupSize[i]; j++) {
                results.add(new PageItem(urls[groupOffset + j], titles[groupOffset + j]));
            }
            groupOffset += groupSize[i];

            groups.add(new PageGroup(groupLabel[i], isAdGroup[i], results));
        }

        assert !GURL.isEmptyOrInvalid(url);
        assert query != null && !query.isEmpty();
        ContinuousNavigationMetadata metadata =
                new ContinuousNavigationMetadata(url, query, resultCategory, groups);
        mListener.onResult(metadata);
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

        mState = State.CAPTURING;
        SearchResultExtractorProducerJni.get().fetchResults(
                mNativeSearchResultExtractorProducer, mTab.getWebContents(), query);
    }

    @Override
    void cancel() {
        if (mState == State.CAPTURING) {
            mState = State.CANCELLED;
        }
    }

    void destroy() {
        if (mNativeSearchResultExtractorProducer == 0) return;

        SearchResultExtractorProducerJni.get().destroy(mNativeSearchResultExtractorProducer);
        mNativeSearchResultExtractorProducer = 0;
    }

    @NativeMethods
    interface Natives {
        long create(SearchResultExtractorProducer producer);
        void fetchResults(
                long nativeSearchResultExtractorProducer, WebContents webContents, String query);
        void destroy(long nativeSearchResultExtractorProducer);
    }
}
