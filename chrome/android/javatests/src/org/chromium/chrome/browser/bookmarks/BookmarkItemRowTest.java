// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.JUnitTestGURLs;

/** Tests for the bookmark item row. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class BookmarkItemRowTest extends BlankUiTestActivityTestCase {
    private static final String TITLE = "BookmarkItemRow";

    @Mock BookmarkModel mModel;
    @Mock BookmarkDelegate mDelegate;
    @Mock SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock DragStateDelegate mDragStateDelegate;
    @Mock BookmarkItem mBookmarkItem;
    @Mock LargeIconBridge mLargeIconBridge;
    @Mock RoundedIconGenerator mRoundedIconGenerator;

    private BookmarkId mBookmarkId;
    private BookmarkItemRow mBookmarkItemRow;
    private ViewGroup mContentView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        doAnswer(
                        (invocation) -> {
                            ((LargeIconCallback) invocation.getArgument(2))
                                    .onLargeIconAvailable(null, 0, false, IconType.FAVICON);
                            return null;
                        })
                .when(mLargeIconBridge)
                .getLargeIconForUrl(any(), anyInt(), any());

        doReturn(mModel).when(mDelegate).getModel();
        doReturn(mSelectionDelegate).when(mDelegate).getSelectionDelegate();
        doReturn(mDragStateDelegate).when(mDelegate).getDragStateDelegate();
        doReturn(mLargeIconBridge).when(mDelegate).getLargeIconBridge();
        mBookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
        doReturn(TITLE).when(mBookmarkItem).getTitle();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mBookmarkItem).getUrl();
        doReturn(JUnitTestGURLs.EXAMPLE_URL.getSpec()).when(mBookmarkItem).getUrlForDisplay();
        doReturn(mBookmarkItem).when(mModel).getBookmarkById(mBookmarkId);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(getActivity());

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);

                    getActivity().setContentView(mContentView, params);
                    mBookmarkItemRow =
                            BookmarkManagerCoordinator.buildBookmarkItemView(mContentView);
                    mBookmarkItemRow.setRoundedIconGeneratorForTesting(mRoundedIconGenerator);
                    mBookmarkItemRow.onDelegateInitialized(mDelegate);
                });
    }

    @Test
    @SmallTest
    public void testSetBookmarkId() {
        doReturn(BookmarkUiMode.FOLDER).when(mDelegate).getCurrentUiMode();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkItemRow.setBookmarkId(mBookmarkId, Location.TOP, false);
                });

        Assert.assertFalse(mBookmarkItemRow.getFaviconCancelledForTesting());

        TextView title = mBookmarkItemRow.findViewById(R.id.title);
        TextView desc = mBookmarkItemRow.findViewById(R.id.description);
        Assert.assertEquals(title.getText(), TITLE);
        Assert.assertEquals(desc.getText(), JUnitTestGURLs.EXAMPLE_URL.getSpec());

        mBookmarkItemRow.onClick();
        verify(mDelegate).openBookmark(mBookmarkId);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testSetBookmarkId_LoadingWhileClicked() {
        doReturn(BookmarkUiMode.LOADING).when(mDelegate).getCurrentUiMode();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkItemRow.setBookmarkId(mBookmarkId, Location.TOP, false);
                });

        mBookmarkItemRow.onClick();
        verify(mDelegate, Mockito.times(0)).openBookmark(mBookmarkId);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testSetBookmarkId_InvalidStateWhileClicked() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkItemRow.setBookmarkId(mBookmarkId, Location.TOP, false);
                });

        mBookmarkItemRow.onClick();
        verify(mDelegate, Mockito.times(0)).openBookmark(mBookmarkId);
    }
}
