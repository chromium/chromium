// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests functionality in {@link BookmarkFolderPickerActivity}. */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class BookmarkFolderPickerActivityTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor ArgumentCaptor<BookmarkItem> mBookmarkItemCaptor;
    @Mock BookmarkModelObserver mBookmarkModelObserver;

    private static BookmarkModel sBookmarkModel;
    private static BookmarkId sMobileFolderId;
    private static BookmarkId sOtherFolderId;
    private static BookmarkId sLocalOrSyncableReadingListFolder;

    private final BookmarkManagerOpener mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();
    private BookmarkFolderPickerActivity mActivity;

    @BeforeClass
    public static void setUpBeforeClass() throws TimeoutException {
        sActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel =
                            BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
                    sBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sMobileFolderId = sBookmarkModel.getMobileFolderId();
                    sOtherFolderId = sBookmarkModel.getOtherFolderId();
                    sLocalOrSyncableReadingListFolder =
                            sBookmarkModel.getLocalOrSyncableReadingListFolder();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel.removeAllUserBookmarks();
                });
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testMoveBookmark() throws ExecutionException, TimeoutException, Exception {
        BookmarkId bookmark =
                addBookmark(sMobileFolderId, 0, "bookmark", new GURL("https://google.com"));
        BookmarkId folder = addFolder(sMobileFolderId, 1, "folder");

        startFolderPickerActivity(bookmark);

        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.addObserver(mBookmarkModelObserver));

        onView(withText("Mobile bookmarks")).perform(click());
        onView(withText("folder")).perform(click());
        onView(withText("Move here")).perform(click());

        BookmarkItem oldParent = getBookmarkItem(sMobileFolderId);
        BookmarkItem newParent = getBookmarkItem(folder);
        verifyBookmarkMoved(oldParent, 0, newParent, 0);
        verifyNoMoreInteractions(mBookmarkModelObserver);

        CriteriaHelper.pollUiThread(() -> mActivity.isFinishing());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testMoveBookmarkToReadingList()
            throws ExecutionException, TimeoutException, Exception {
        String title = "bookmark";
        GURL url = new GURL("https://google.com");

        BookmarkId bookmark = addBookmark(sMobileFolderId, 0, title, url);
        startFolderPickerActivity(bookmark);

        ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.addObserver(mBookmarkModelObserver));

        onView(withId(R.id.folder_recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(withText("Reading list"))));
        onView(withText("Reading list")).perform(click());
        onView(withText("Move here")).perform(click());

        BookmarkItem oldParent = getBookmarkItem(sMobileFolderId);
        BookmarkItem newParent = getBookmarkItem(sLocalOrSyncableReadingListFolder);
        verifyBookmarkMoved(oldParent, 0, newParent, 0);
        verifyNoMoreInteractions(mBookmarkModelObserver);

        CriteriaHelper.pollUiThread(() -> mActivity.isFinishing());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testCancelButton()
            throws ExecutionException, TimeoutException, InterruptedException {
        BookmarkId bookmark =
                addBookmark(sMobileFolderId, 0, "bookmark", new GURL("https://google.com"));
        startFolderPickerActivity(bookmark);

        onView(withText("Cancel")).perform(click());

        CriteriaHelper.pollUiThread(() -> mActivity.isFinishing());
    }

    private BookmarkItem getBookmarkItem(BookmarkId bookmarkId) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.getBookmarkById(bookmarkId));
    }

    private BookmarkId addFolder(BookmarkId parent, int index, String title)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.addFolder(parent, index, title));
    }

    private BookmarkId addBookmark(BookmarkId parent, int index, String title, GURL url)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.addBookmark(parent, index, title, url));
    }

    private void startFolderPickerActivity(BookmarkId... ids) {
        mActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        BookmarkFolderPickerActivity.class,
                        Stage.RESUMED,
                        () -> {
                            mBookmarkManagerOpener.startFolderPickerActivity(
                                    sActivityTestRule.getActivity(),
                                    sActivityTestRule.getProfile(false),
                                    ids);
                        });
    }

    private void verifyBookmarkMoved(
            BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex) {
        verify(mBookmarkModelObserver)
                .bookmarkNodeMoved(
                        mBookmarkItemCaptor.capture(), eq(0), mBookmarkItemCaptor.capture(), eq(0));

        List<BookmarkItem> capturedItems = mBookmarkItemCaptor.getAllValues();
        assertEquals(oldParent.getId(), capturedItems.get(0).getId());
        assertEquals(oldParent.getTitle(), capturedItems.get(0).getTitle());
        assertEquals(newParent.getId(), capturedItems.get(1).getId());
        assertEquals(newParent.getTitle(), capturedItems.get(1).getTitle());
    }
}
