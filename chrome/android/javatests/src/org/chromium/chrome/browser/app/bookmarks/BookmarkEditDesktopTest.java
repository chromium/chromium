// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkEditMetrics.BookmarkEditOutcome;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkModelTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests functionality in BookmarkEditActivity for Desktop dialog. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG})
public class BookmarkEditDesktopTest {

    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mActivityTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    private static final String TITLE_A = "a";
    private static final String TITLE_B = "b";
    private static final String URL_A = "http://a.com/";
    private static final String URL_B = "http://b.com/";

    private BookmarkModel mBookmarkModel;
    private BookmarkModelObserver mModelObserver;
    private BookmarkId mBookmarkId;
    private BookmarkId mMobileNode;
    private BookmarkEditActivity mActivity;
    private WebPageStation mBlankPage;

    private final CallbackHelper mDestroyedCallback = new CallbackHelper();
    private final ActivityStateListener mActivityStateListener =
            (activity, newState) -> {
                if (newState == ActivityState.DESTROYED) mDestroyedCallback.notifyCalled();
            };
    private final CallbackHelper mModelChangedCallback = new CallbackHelper();

    @Before
    public void setUp() throws TimeoutException {
        DeviceInfo.setIsDesktopForTesting(true);

        mBlankPage = mActivityTestRule.start();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel =
                            BookmarkModel.getForProfile(mActivityTestRule.getProfile(false));
                    mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMobileNode = mBookmarkModel.getMobileFolderId();
                });
        mBookmarkId =
                BookmarkModelTest.addBookmark(
                        mBookmarkModel, mMobileNode, 0, TITLE_A, new GURL(URL_A));

        mModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        mModelChangedCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkModel.addObserver(mModelObserver));

        startEditActivity(mBookmarkId);
        CriteriaHelper.pollUiThread(
                () -> mActivity.getCloseButton() != null,
                "Close button not initialized in options menu");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            mActivityStateListener, mActivity);
                });
    }

    @After
    public void tearDown() throws ExecutionException {
        if (mActivity != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
                        if (!mActivity.isFinishing() && !mActivity.isDestroyed()) {
                            mActivity.finish();
                        }
                    });
        }
        if (mBookmarkModel != null && mModelObserver != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mBookmarkModel.removeObserver(mModelObserver);
                        mBookmarkModel.removeAllUserBookmarks();
                    });
        }
        DeviceInfo.resetIsDesktopForTesting();
    }

    private void startEditActivity(BookmarkId bookmarkId) {
        mActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        BookmarkEditActivity.class,
                        Stage.RESUMED,
                        () -> {
                            new BookmarkManagerOpenerImpl()
                                    .startEditActivity(
                                            mActivityTestRule.getActivity(),
                                            mActivityTestRule.getProfile(false),
                                            bookmarkId);
                        });
    }

    private BookmarkItem getBookmarkItem(BookmarkId bookmarkId) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkModel.getBookmarkById(bookmarkId));
    }

    @Test
    @MediumTest
    public void testDesktopDialogInflated() {
        Assert.assertNotNull("Activity should not be null", mActivity);
        Assert.assertNotNull("Save button should exist", mActivity.getSaveButton());
        Assert.assertNotNull("Remove button should exist", mActivity.getRemoveButton());
        Assert.assertNotNull("Close button should exist in menu", mActivity.getCloseButton());
        Assert.assertNull("Delete button should not exist in menu", mActivity.getDeleteButton());
    }

    @Test
    @MediumTest
    public void testSaveButton() throws ExecutionException, TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.SAVED)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.getTitleEditText().getEditText().setText(TITLE_B);
                    mActivity.getUrlEditText().getEditText().setText(URL_B);
                });

        Assert.assertEquals(View.VISIBLE, mActivity.getSaveButton().getVisibility());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.getSaveButton().performClick());

        mDestroyedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertEquals("Title should be updated", TITLE_B, bookmarkItem.getTitle());
        Assert.assertEquals("URL should be updated", URL_B, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testRemoveButton() throws ExecutionException, TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.DELETED)
                        .build();

        Assert.assertEquals(View.VISIBLE, mActivity.getRemoveButton().getVisibility());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.getRemoveButton().performClick());

        mDestroyedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertNull("Bookmark should be deleted", bookmarkItem);
    }

    @Test
    @MediumTest
    public void testCloseButton() throws ExecutionException, TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.CLOSED)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.getTitleEditText().getEditText().setText(TITLE_B);
                    mActivity.getUrlEditText().getEditText().setText(URL_B);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.onOptionsItemSelected(mActivity.getCloseButton());
                });

        mDestroyedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertEquals("Title should NOT be updated", TITLE_A, bookmarkItem.getTitle());
        Assert.assertEquals("URL should NOT be updated", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testDismissed() throws ExecutionException, TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome",
                                BookmarkEditOutcome.DISMISSED)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.finish());
        mDestroyedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testOnStopDoesNotSave() throws ExecutionException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.getTitleEditText().getEditText().setText(TITLE_B);
                    mActivity.getUrlEditText().getEditText().setText(URL_B);
                    mActivity.onStop();
                });

        BookmarkItem bookmarkItem = getBookmarkItem(mBookmarkId);
        Assert.assertEquals("Title should NOT be updated", TITLE_A, bookmarkItem.getTitle());
        Assert.assertEquals("URL should NOT be updated", URL_A, bookmarkItem.getUrl().getSpec());
    }
}
