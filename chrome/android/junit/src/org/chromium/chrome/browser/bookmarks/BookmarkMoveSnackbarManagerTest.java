// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlockingNoException;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.Arrays;

@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
/** Unit tests for {@link BookmarkMoveSnackbarManager}. */
public class BookmarkMoveSnackbarManagerTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesRule = new Features.JUnitProcessor();

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private BookmarkFolderPickerActivity mFolderPickerActivity;

    private BookmarkMoveSnackbarManager mBookmarkMoveSnackbarManager;
    private Activity mActivity;
    private BookmarkModel mBookmarkModel;
    private BookmarkId mBookmarkId1;
    private BookmarkId mBookmarkId2;
    private BookmarkId mBookmarkId3;
    private BookmarkId mFolderId;
    private BookmarkId mMobileFolderId;
    private BookmarkId mAccountMobileFolderId;
    private BookmarkModelObserver mBookmarkModelObserver;
    private CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId("test@gmail.com", "testGaiaId");

    @Before
    public void setUp() {
        mBookmarkModel = setupFakeBookmarkModel();
        doReturn(mAccountInfo).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        doReturn(true).when(mSnackbarManager).canShowSnackbar();

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;

        mBookmarkMoveSnackbarManager =
                new BookmarkMoveSnackbarManager(
                        mActivity, mBookmarkModel, mSnackbarManager, mIdentityManager);
        mBookmarkModelObserver = mBookmarkMoveSnackbarManager.getBookmarkModelObserverForTesting();
    }

    private BookmarkModel setupFakeBookmarkModel() {
        BookmarkModel bookmarkModel = FakeBookmarkModel.createModel();
        mAccountMobileFolderId =
                runOnUiThreadBlockingNoException(() -> bookmarkModel.getAccountMobileFolderId());
        mMobileFolderId = runOnUiThreadBlockingNoException(() -> bookmarkModel.getMobileFolderId());
        mBookmarkId1 =
                runOnUiThreadBlockingNoException(
                        () ->
                                bookmarkModel.addBookmark(
                                        mMobileFolderId, 0, "bookmark 1", new GURL("test1.com")));
        mBookmarkId2 =
                runOnUiThreadBlockingNoException(
                        () ->
                                bookmarkModel.addBookmark(
                                        mMobileFolderId, 0, "bookmark 2", new GURL("test2.com")));
        mBookmarkId3 =
                runOnUiThreadBlockingNoException(
                        () ->
                                bookmarkModel.addBookmark(
                                        mMobileFolderId, 0, "bookmark 3", new GURL("test3.com")));
        mFolderId =
                runOnUiThreadBlockingNoException(
                        () -> bookmarkModel.addFolder(mMobileFolderId, 0, "local folder"));

        return bookmarkModel;
    }

    @Test
    @SmallTest
    public void testWithoutAnyMovement() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(mBookmarkId1);
        verifyNoInteractions(mSnackbarManager);

        mBookmarkMoveSnackbarManager.onActivityStateChange(
                mFolderPickerActivity, ActivityState.DESTROYED);
        verifyNoInteractions(mSnackbarManager);

        // Subsequent move events shouldn't be captured.
        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0);
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mSnackbarManager);
    }

    @Test
    @SmallTest
    public void testSingleLocalMovement() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(mBookmarkId1);

        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0);
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);

        ArgumentCaptor<Snackbar> mSnackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Bookmark saved to Mobile bookmarks. It is only saved to this device.",
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    public void testSingleAccountMovement() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(mBookmarkId1);

        mBookmarkModel.moveBookmark(mBookmarkId1, mAccountMobileFolderId, 0);
        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0);
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);

        ArgumentCaptor<Snackbar> mSnackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Bookmark saved to Mobile bookmarks in your account, test@gmail.com.",
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    public void testMultipleLocalMovement() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(
                mBookmarkId1, mBookmarkId2, mBookmarkId3);

        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0);
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);

        ArgumentCaptor<Snackbar> mSnackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Bookmarks saved to Mobile bookmarks. It is only saved to this device.",
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    public void testMultipleAccountMovement() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(
                mBookmarkId1, mBookmarkId2, mBookmarkId3);

        mBookmarkModel.moveBookmarks(
                Arrays.asList(mBookmarkId1, mBookmarkId2, mBookmarkId3), mAccountMobileFolderId);
        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0);
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);

        ArgumentCaptor<Snackbar> mSnackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Bookmarks saved to Mobile bookmarks in your account, test@gmail.com.",
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    @DisableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testMovementWithoutFeatureFlag() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(mBookmarkId1);

        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0);
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mSnackbarManager);
    }

    @Test
    @SmallTest
    public void testSnackbarAvailability() {
        mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(mBookmarkId1);

        mBookmarkModelObserver.bookmarkNodeMoved(
                mBookmarkModel.getBookmarkById(mAccountMobileFolderId),
                0,
                mBookmarkModel.getBookmarkById(mMobileFolderId),
                0);
        doReturn(false).when(mSnackbarManager).canShowSnackbar();
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        doReturn(true).when(mSnackbarManager).canShowSnackbar();
        mBookmarkMoveSnackbarManager.onActivityStateChange(mActivity, ActivityState.RESUMED);

        ArgumentCaptor<Snackbar> mSnackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Bookmark saved to Mobile bookmarks. It is only saved to this device.",
                snackbar.getTextForTesting());
    }
}
