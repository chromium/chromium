// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.MenuItem;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkModelTest;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests functionality in BookmarkEditActivity.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BookmarkEditTest {
    private static final String TITLE_A = "a";
    private static final String TITLE_B = "b";
    private static final String URL_A = "http://a.com/";
    private static final String URL_B = "http://b.com/";
    private static final String FOLDER_A = "FolderA";

    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private BookmarkModel mBookmarkModel;
    private BookmarkModelObserver mModelObserver;
    private CallbackHelper mModelChangedCallback = new CallbackHelper();
    private BookmarkId mBookmarkId;
    private BookmarkId mMobileNode;
    private BookmarkId mOtherNode;
    private BookmarkEditActivity mBookmarkEditActivity;

    private CallbackHelper mDestroyedCallback = new CallbackHelper();
    private ActivityStateListener mActivityStateListener = new ActivityStateListener() {
        @Override
        public void onActivityStateChange(Activity activity, int newState) {
            if (newState == ActivityState.DESTROYED) mDestroyedCallback.notifyCalled();
        }
    };

    @Before
    public void setUp() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel = BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
            mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
        });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMobileNode = mBookmarkModel.getMobileFolderId();
            mOtherNode = mBookmarkModel.getOtherFolderId();
        });
        mBookmarkId = BookmarkModelTest.addBookmark(
                mBookmarkModel, mMobileNode, 0, TITLE_A, new GURL(URL_A));

        mModelObserver = new BookmarkModelObserver() {
            @Override
            public void bookmarkModelChanged() {
                mModelChangedCallback.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> mBookmarkModel.addObserver(mModelObserver));

        startEditActivity(mBookmarkId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.registerStateListenerForActivity(
                    mActivityStateListener, mBookmarkEditActivity);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.removeObserver(mModelObserver);
            mBookmarkModel.removeAllUserBookmarks();
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        });
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditTitleAndUrl() throws ExecutionException, TimeoutException {
        Assert.assertEquals("Incorrect title.", TITLE_A,
                mBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals("Incorrect url.", URL_A,
                mBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkEditActivity.getTitleEditText().getEditText().setText(TITLE_B);
            mBookmarkEditActivity.getUrlEditText().getEditText().setText(URL_B);
            mBookmarkEditActivity.finish();
        });
        mDestroyedCallback.waitForCallback(0);

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_B, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_B, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditEmptyInputRejected() throws ExecutionException, TimeoutException {
        Assert.assertEquals("Incorrect title.", TITLE_A,
                mBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals("Incorrect url.", URL_A,
                mBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkEditActivity.getTitleEditText().getEditText().setText("");
            mBookmarkEditActivity.getUrlEditText().getEditText().setText("");
            mBookmarkEditActivity.finish();
        });
        mDestroyedCallback.waitForCallback(0);

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_A, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testInvalidUrlRejected() throws ExecutionException, TimeoutException {
        Assert.assertEquals("Incorrect url.", URL_A,
                mBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkEditActivity.getUrlEditText().getEditText().setText("http:://?foo=bar");
            mBookmarkEditActivity.finish();
        });
        mDestroyedCallback.waitForCallback(0);

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertEquals("Incorrect url after edit.", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditActivityDeleteButton() throws ExecutionException, TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkEditActivity.onOptionsItemSelected(mBookmarkEditActivity.getDeleteButton());
        });
        mDestroyedCallback.waitForCallback(0);

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertNull("Bookmark item should have been deleted.", bookmarkItem);
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditActivityHomeButton() throws ExecutionException {
        MenuItem item = Mockito.mock(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(android.R.id.home);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkEditActivity.onOptionsItemSelected(item));

        Assert.assertTrue("BookmarkActivity should be finishing or destroyed.",
                mBookmarkEditActivity.isFinishing() || mBookmarkEditActivity.isDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditActivityReflectsModelChanges() throws TimeoutException, ExecutionException {
        Assert.assertEquals("Incorrect title.", TITLE_A,
                mBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals("Incorrect folder.", getBookmarkItem(mMobileNode).getTitle(),
                mBookmarkEditActivity.getFolderTextView().getText());

        int currentModelChangedCount = mModelChangedCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.setBookmarkTitle(mBookmarkId, TITLE_B);
            mBookmarkModel.moveBookmark(mBookmarkId, mOtherNode, 0);
        });
        mModelChangedCallback.waitForCallback(currentModelChangedCount);

        Assert.assertEquals("Title shouldn't change after model update.", TITLE_A,
                mBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals("Folder should change after model update.",
                getBookmarkItem(mOtherNode).getTitle(),
                mBookmarkEditActivity.getFolderTextView().getText());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditActivityFinishesWhenBookmarkDeleted() throws TimeoutException {
        int currentModelChangedCount = mModelChangedCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> mBookmarkModel.deleteBookmark(mBookmarkId));
        mModelChangedCallback.waitForCallback(currentModelChangedCount);

        Assert.assertTrue("BookmarkActivity should be finishing or destroyed.",
                mBookmarkEditActivity.isFinishing() || mBookmarkEditActivity.isDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditFolderLocation() throws ExecutionException, TimeoutException {
        BookmarkId testFolder = addFolder(mMobileNode, 0, FOLDER_A);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkEditActivity.getFolderTextView().performClick());
        waitForMoveFolderActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkFolderSelectActivity folderSelectActivity =
                    (BookmarkFolderSelectActivity)
                            ApplicationStatus.getLastTrackedFocusedActivity();
            int pos = folderSelectActivity.getFolderPositionForTesting(testFolder);
            Assert.assertNotEquals("Didn't find position for test folder.", -1, pos);
            folderSelectActivity.performClickForTesting(pos);
        });

        waitForEditActivity();
        Assert.assertEquals("Folder should change after folder activity finishes.", FOLDER_A,
                mBookmarkEditActivity.getFolderTextView().getText());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testChangeFolderWhenBookmarkRemoved() throws ExecutionException, TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkEditActivity.getFolderTextView().performClick());
        waitForMoveFolderActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Expected BookmarkFolderSelectActivity.",
                    ApplicationStatus.getLastTrackedFocusedActivity()
                                    instanceof BookmarkFolderSelectActivity);
            mBookmarkModel.deleteBookmark(mBookmarkId);
        });
        // clang-format off
        CriteriaHelper.pollUiThread(() ->
                !(ApplicationStatus.getLastTrackedFocusedActivity()
                      instanceof BookmarkFolderSelectActivity),
                "Timed out waiting for BookmarkFolderSelectActivity to close");
        // clang-format on
    }

    private BookmarkItem getBookmarkItem(BookmarkId bookmarkId) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.getBookmarkById(bookmarkId));
    }

    private void startEditActivity(BookmarkId bookmarkId) {
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mBookmarkEditActivity = (BookmarkEditActivity) InstrumentationRegistry.getInstrumentation()
                                        .startActivitySync(intent);
    }

    private BookmarkId addFolder(BookmarkId parent, int index, String title)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(parent, index, title));
    }

    private void waitForMoveFolderActivity() {
        // clang-format off
        CriteriaHelper.pollUiThread(()->
                ApplicationStatus.getLastTrackedFocusedActivity()
                    instanceof BookmarkFolderSelectActivity,
                "Timed out waiting for BookmarkFolderSelectActivity");
        // clang-format on
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void waitForEditActivity() {
        // clang-format off
        CriteriaHelper.pollUiThread(()->
                ApplicationStatus.getLastTrackedFocusedActivity() instanceof BookmarkEditActivity,
                "Timed out waiting for BookmarkEditActivity");
        // clang-format on
        mBookmarkEditActivity =
                (BookmarkEditActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
