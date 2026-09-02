// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

/** Unit tests for {@link BookmarkUndoController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkUndoControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManager mSnackbarManager1;
    @Mock private SnackbarManager mSnackbarManager2;

    private final Context mContext =
            new ContextThemeWrapper(
                    ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    private final FakeBookmarkModel mBookmarkModel = FakeBookmarkModel.createModel();
    private BookmarkUndoController mUndoController1;
    private BookmarkUndoController mUndoController2;

    @Before
    public void setUp() {
        mUndoController1 = new BookmarkUndoController(mContext, mBookmarkModel, mSnackbarManager1);
        mUndoController2 = new BookmarkUndoController(mContext, mBookmarkModel, mSnackbarManager2);
    }

    @Test
    public void testSingleWindowSnackbarSpawning() {
        BookmarkItem parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        BookmarkId bookmark1 =
                mBookmarkModel.addBookmark(
                        parent.getId(), 0, "Window 1 bookmark", new GURL("https://test.com"));

        // Deleting with mUndoController1 as the originator should ONLY notify mUndoController1.
        mBookmarkModel.deleteBookmarks(mUndoController1, bookmark1);

        verify(mSnackbarManager1, times(1)).showSnackbar(any(Snackbar.class));
        verify(mSnackbarManager2, never()).showSnackbar(any(Snackbar.class));

        // Deleting with null originator should notify all registered observers (both controllers).
        BookmarkId bookmark2 =
                mBookmarkModel.addBookmark(
                        parent.getId(), 0, "Window 2 bookmark", new GURL("https://test.com"));
        mBookmarkModel.deleteBookmarks((BookmarkModel.BookmarkDeleteObserver) null, bookmark2);

        verify(mSnackbarManager1, times(2)).showSnackbar(any(Snackbar.class));
        verify(mSnackbarManager2, times(1)).showSnackbar(any(Snackbar.class));

        mUndoController1.destroy();
        mUndoController2.destroy();
    }

    @Test
    public void testStaleSnackbarDismissalOnSuccessiveDeletions() {
        InOrder inOrder = inOrder(mSnackbarManager1);

        BookmarkItem parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        BookmarkId bookmark1 =
                mBookmarkModel.addBookmark(
                        parent.getId(), 0, "Bookmark 1", new GURL("https://test1.com"));
        BookmarkId bookmark2 =
                mBookmarkModel.addBookmark(
                        parent.getId(), 0, "Bookmark 2", new GURL("https://test2.com"));

        // First deletion: verify dismissSnackbars is called before showSnackbar.
        mBookmarkModel.deleteBookmarks(mUndoController1, bookmark1);
        inOrder.verify(mSnackbarManager1).dismissSnackbars(mUndoController1);
        inOrder.verify(mSnackbarManager1).showSnackbar(any(Snackbar.class));

        // Second successive deletion without dismissing: verify dismissSnackbars is called
        // before showing the new snackbar, removing stale snackbars.
        mBookmarkModel.deleteBookmarks(mUndoController1, bookmark2);
        inOrder.verify(mSnackbarManager1).dismissSnackbars(mUndoController1);
        inOrder.verify(mSnackbarManager1).showSnackbar(any(Snackbar.class));

        mUndoController1.destroy();
    }

    @Test
    public void testStaleSnackbarDismissalAcrossDifferentControllers() {
        BookmarkUndoController controllerOnSameManager =
                new BookmarkUndoController(mContext, mBookmarkModel, mSnackbarManager1);

        InOrder inOrder = inOrder(mSnackbarManager1);

        BookmarkItem parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        BookmarkId bookmark1 =
                mBookmarkModel.addBookmark(
                        parent.getId(), 0, "Bookmark 1", new GURL("https://test1.com"));
        BookmarkId bookmark2 =
                mBookmarkModel.addBookmark(
                        parent.getId(), 0, "Bookmark 2", new GURL("https://test2.com"));

        mBookmarkModel.deleteBookmarks(mUndoController1, bookmark1);
        inOrder.verify(mSnackbarManager1).dismissSnackbars(mUndoController1);
        inOrder.verify(mSnackbarManager1).showSnackbar(any(Snackbar.class));

        mBookmarkModel.deleteBookmarks(controllerOnSameManager, bookmark2);
        inOrder.verify(mSnackbarManager1).dismissSnackbars(controllerOnSameManager);
        inOrder.verify(mSnackbarManager1).showSnackbar(any(Snackbar.class));

        mUndoController1.destroy();
        controllerOnSameManager.destroy();
    }
}
