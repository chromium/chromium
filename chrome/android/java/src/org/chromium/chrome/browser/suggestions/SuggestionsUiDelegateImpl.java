// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import androidx.annotation.Nullable;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.snippets.EmptySuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

import java.util.ArrayList;
import java.util.List;

/**
 * {@link SuggestionsUiDelegate} implementation.
 */
public class SuggestionsUiDelegateImpl implements SuggestionsUiDelegate {
    private final List<DestructionObserver> mDestructionObservers = new ArrayList<>();
    private SuggestionsSource mSuggestionsSource;
    private final SuggestionsRanker mSuggestionsRanker;
    private final SuggestionsEventReporter mSuggestionsEventReporter;
    private final SuggestionsNavigationDelegate mSuggestionsNavigationDelegate;
    private final NativePageHost mHost;
    private final ImageFetcher mImageFetcher;
    private final SnackbarManager mSnackbarManager;

    private final DiscardableReferencePool mReferencePool;

    private boolean mIsDestroyed;

    public SuggestionsUiDelegateImpl(SuggestionsSource suggestionsSource,
            SuggestionsEventReporter eventReporter,
            SuggestionsNavigationDelegate navigationDelegate, Profile profile, NativePageHost host,
            DiscardableReferencePool referencePool, SnackbarManager snackbarManager) {
        mSuggestionsSource = suggestionsSource;
        mSuggestionsRanker = new SuggestionsRanker();
        mSuggestionsEventReporter = eventReporter;
        mSuggestionsNavigationDelegate = navigationDelegate;
        mImageFetcher = new ImageFetcher(suggestionsSource, profile, referencePool);
        mSnackbarManager = snackbarManager;

        mHost = host;
        mReferencePool = referencePool;
    }

    @Override
    public SuggestionsSource getSuggestionsSource() {
        return mSuggestionsSource;
    }

    @Override
    public SuggestionsRanker getSuggestionsRanker() {
        return mSuggestionsRanker;
    }

    @Nullable
    @Override
    public SuggestionsEventReporter getEventReporter() {
        return mSuggestionsEventReporter;
    }

    @Nullable
    @Override
    public SuggestionsNavigationDelegate getNavigationDelegate() {
        return mSuggestionsNavigationDelegate;
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @Override
    public ImageFetcher getImageFetcher() {
        return mImageFetcher;
    }

    @Override
    public DiscardableReferencePool getReferencePool() {
        return mReferencePool;
    }

    @Override
    public void addDestructionObserver(DestructionObserver destructionObserver) {
        mDestructionObservers.add(destructionObserver);
    }

    @Override
    public boolean isVisible() {
        return mHost.isVisible();
    }

    /** Invalidates the delegate and calls the registered destruction observers. */
    public void onDestroy() {
        assert !mIsDestroyed;

        mImageFetcher.onDestroy();

        for (DestructionObserver observer : mDestructionObservers) observer.onDestroy();

        // SuggestionsSource is not registered with the rest of the destruction observers but
        // instead explicitly destroyed last so that the other destruction observers can use it
        // while they are called.
        mSuggestionsSource.destroy();

        // Now replacing suggestions source with an empty one, which serves as a sentinel here.
        // This prevents crashes when SnippetsBridge being access after being destroyed.
        mSuggestionsSource = new EmptySuggestionsSource();

        mIsDestroyed = true;
    }
}
