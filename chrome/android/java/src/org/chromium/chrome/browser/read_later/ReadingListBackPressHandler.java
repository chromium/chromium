// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * A {@link BackPressHandler} intercepting back press when the current selected tab is launched
 * from reading list.
 */
public class ReadingListBackPressHandler implements BackPressHandler, Destroyable {
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityTabTabObserver mActivityTabTabObserver;
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;

    private BookmarkId mLastUsedParent;

    public ReadingListBackPressHandler(
            ActivityTabProvider activityTabProvider,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier) {
        mActivityTabProvider = activityTabProvider;
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider, true) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        onBackPressStateChanged();

                        // If this tab should intercept back press, start the process of tracking
                        // the last url so that it can be reopened.
                        if (shouldInterceptBackPress()) {
                            new OneShotCallback<>(
                                    bookmarkModelSupplier,
                                    ReadingListBackPressHandler.this::setupLastUsedState);
                        }
                    }
                };
        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    // After {@link BookmarkModel} is available, load it then query the last used URL and store it
    // in a {@link BookmarkId} which will be used to handle the back press.
    private void setupLastUsedState(BookmarkModel bookmarkModel) {
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    // Note: there's a slight (but unlikely) chance the the user changed the last
                    // used url prior
                    // to tracking it here.
                    BookmarkUiState lastUsedState =
                            BookmarkUiState.createStateFromUrl(
                                    BookmarkUtils.getLastUsedUrl(), bookmarkModel);
                    mLastUsedParent = lastUsedState.getFolder();
                });
    }

    @Override
    public @BackPressResult int handleBackPress() {
        Tab tab = mActivityTabProvider.get();
        int result = shouldInterceptBackPress() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        if (mBookmarkModelSupplier.hasValue()
                && mBookmarkModelSupplier.get().areAccountBookmarkFoldersActive()) {
            BookmarkUtils.showBookmarkManager(null, mLastUsedParent, tab.isIncognito());
        } else {
            ReadingListUtils.showReadingList(tab.isIncognito());
        }

        WebContents webContents = tab.getWebContents();
        if (webContents != null) webContents.dispatchBeforeUnload(false);
        return result;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        mActivityTabTabObserver.destroy();
    }

    private void onBackPressStateChanged() {
        mBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    private boolean shouldInterceptBackPress() {
        Tab tab = mActivityTabProvider.get();
        return tab != null && tab.getLaunchType() == TabLaunchType.FROM_READING_LIST;
    }
}
