// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * A {@link BackPressHandler} intercepting back press when the current selected tab is launched from
 * reading list.
 */
@NullMarked
public class ReadingListBackPressHandler implements BackPressHandler, Destroyable {
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final Activity mActivity;
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityTabTabObserver mActivityTabTabObserver;
    private final ObservableSupplier<BookmarkManagerOpener> mBookmarkManagerOpenerSupplier;

    private @Nullable BookmarkId mLastUsedParent;

    /**
     * @param activity The android activity.
     * @param activityTabProvider Provides the current activity tab.
     * @param bookmarkModelSupplier Supplier the BookmarkModel, used to get the last used url of the
     *     bookmarks manager.
     * @param bookmarkManagerOpenerSupplier Supplies the BookmarkManagerOpenerSupplier, used to open
     *     the bookmarks manager.
     */
    public ReadingListBackPressHandler(
            Activity activity,
            ActivityTabProvider activityTabProvider,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            ObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier) {
        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider, true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab, boolean hint) {
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
        mBookmarkManagerOpenerSupplier = bookmarkManagerOpenerSupplier;
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
                                    mBookmarkManagerOpenerSupplier.get().getLastUsedUrl(),
                                    bookmarkModel);
                    mLastUsedParent = lastUsedState.getFolder();
                });
    }

    @Override
    public @BackPressResult int handleBackPress() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null) {
            return BackPressResult.FAILURE;
        }
        int result = shouldInterceptBackPress() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        // If the last used parent failed to load somehow, default to the local reading list.
        if (mLastUsedParent == null) {
            mLastUsedParent = new BookmarkId(/* id= */ 0, BookmarkType.READING_LIST);
        }
        mBookmarkManagerOpenerSupplier
                .get()
                .showBookmarkManager(mActivity, tab, tab.getProfile(), mLastUsedParent);

        WebContents webContents = tab.getWebContents();
        if (webContents != null) webContents.dispatchBeforeUnload(false);
        return result;
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        // Escape key presses should not close the tab even if it was opened from the reading list.
        // We do not also implement a custom {@link BackPressHandler#handleEscPress()} since we
        // don't want anything to happen and for the manager to move to the next priority handler.
        return false;
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
