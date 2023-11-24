// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

import java.util.ArrayList;
import java.util.List;

/** {@link SuggestionsUiDelegate} implementation. */
public class SuggestionsUiDelegateImpl implements SuggestionsUiDelegate {
    private final List<DestructionObserver> mDestructionObservers = new ArrayList<>();
    private final SuggestionsNavigationDelegate mSuggestionsNavigationDelegate;
    private final NativePageHost mHost;
    private final ImageFetcher mImageFetcher;
    private final SnackbarManager mSnackbarManager;

    private boolean mIsDestroyed;

    public SuggestionsUiDelegateImpl(
            SuggestionsNavigationDelegate navigationDelegate,
            Profile profile,
            NativePageHost host,
            SnackbarManager snackbarManager) {
        mSuggestionsNavigationDelegate = navigationDelegate;
        mImageFetcher = new ImageFetcher(profile);
        mSnackbarManager = snackbarManager;

        mHost = host;
    }

    @Nullable
    @Override
    public SuggestionsNavigationDelegate getNavigationDelegate() {
        return mSuggestionsNavigationDelegate;
    }

    @Override
    public NativePageHost getNativePageHost() {
        return mHost;
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
        mDestructionObservers.clear();

        mIsDestroyed = true;
    }
}
