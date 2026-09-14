// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkDesktopPopupMetrics.BookmarkDesktopPopupOutcome;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BookmarkPopupMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkPopupMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private Profile mProfile;

    @Mock private ShoppingService mShoppingService;
    @Mock private PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;
    @Mock private PriceDropNotificationManager mPriceDropNotificationManager;

    @Mock private Runnable mDismissRunnable;

    @Captor private ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;

    private final BookmarkId mBookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
    private final BookmarkId mParentId = new BookmarkId(2, BookmarkType.NORMAL);

    private BookmarkItem mBookmarkItem;
    private BookmarkItem mParentItem;

    private Activity mActivity;
    private PropertyModel mPropertyModel;
    private BookmarkPopupMediator mMediator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mPropertyModel = new PropertyModel(BookmarkPopupProperties.ALL_KEYS);

        mBookmarkItem =
                new BookmarkItem(
                        mBookmarkId,
                        "Test Bookmark",
                        /* url= */ null,
                        /* isFolder= */ false,
                        mParentId,
                        /* isEditable= */ false,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);
        mParentItem =
                new BookmarkItem(
                        mParentId,
                        "Test Folder",
                        /* url= */ null,
                        /* isFolder= */ true,
                        /* parentId= */ null,
                        /* isEditable= */ false,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);

        when(mBookmarkModel.getBookmarkById(mBookmarkId)).thenReturn(mBookmarkItem);
        PriceTrackingUtilsJni.setInstanceForTesting(mMockPriceTrackingUtilsJni);
        when(mBookmarkModel.getBookmarkById(mParentId)).thenReturn(mParentItem);

        mMediator =
                new BookmarkPopupMediator(
                        mPropertyModel,
                        mBookmarkModel,
                        mBookmarkManagerOpener,
                        mBookmarkImageFetcher,
                        mActivity,
                        mProfile,
                        mShoppingService,
                        mPriceDropNotificationManager,
                        mDismissRunnable);
    }

    @Test
    public void testShowAndModelBinding() {
        doAnswer(
                        invocation -> {
                            Callback<Boolean> cb = invocation.getArgument(2);
                            cb.onResult(true);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(Mockito.any(), Mockito.anyLong(), Mockito.any());
        mMediator.show(mBookmarkId, true);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        assertEquals(
                mActivity.getString(R.string.bookmark_added),
                mPropertyModel.get(BookmarkPopupProperties.HEADER_TEXT));
        assertEquals("Test Bookmark", mPropertyModel.get(BookmarkPopupProperties.TITLE));
        assertEquals("Test Folder", mPropertyModel.get(BookmarkPopupProperties.FOLDER_NAME));
        verify(mBookmarkImageFetcher).fetchImageForBookmarkWithFaviconFallback(any(), eq(0), any());
    }

    @Test
    public void testShowAndModelBinding_EditMode() {
        mMediator.show(mBookmarkId, false);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        assertEquals(
                mActivity.getString(R.string.edit_bookmark),
                mPropertyModel.get(BookmarkPopupProperties.HEADER_TEXT));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testShow_ImageFetching() {
        doAnswer(
                        invocation -> {
                            Callback<Boolean> cb = invocation.getArgument(2);
                            cb.onResult(true);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(Mockito.any(), Mockito.anyLong(), Mockito.any());
        mMediator.show(mBookmarkId, true);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mBookmarkImageFetcher)
                .fetchImageForBookmarkWithFaviconFallback(any(), eq(0), mCallbackCaptor.capture());

        Drawable mockDrawable = mock(Drawable.class);
        mCallbackCaptor.getValue().onResult(mockDrawable);

        assertEquals(mockDrawable, mPropertyModel.get(BookmarkPopupProperties.IMAGE_DRAWABLE));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testShow_DestroyedBeforeCallback() {
        doAnswer(
                        invocation -> {
                            Callback<Boolean> cb = invocation.getArgument(2);
                            cb.onResult(true);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(Mockito.any(), Mockito.anyLong(), Mockito.any());
        mMediator.show(mBookmarkId, true);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        verify(mBookmarkImageFetcher)
                .fetchImageForBookmarkWithFaviconFallback(any(), eq(0), mCallbackCaptor.capture());

        // Destroy mediator before callback returns
        mMediator.destroy();

        Drawable mockDrawable = mock(Drawable.class);
        mCallbackCaptor.getValue().onResult(mockDrawable);

        // Properties should not be set since mediator is destroyed and callback is cancelled
        assertNull(mPropertyModel.get(BookmarkPopupProperties.IMAGE_DRAWABLE));
    }

    @Test
    public void testShow_DestroyedBeforeModelLoaded() {
        doAnswer(
                        invocation -> {
                            Callback<Boolean> cb = invocation.getArgument(2);
                            cb.onResult(true);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(Mockito.any(), Mockito.anyLong(), Mockito.any());
        mMediator.show(mBookmarkId, true);

        // Destroy mediator before model loading callback runs
        mMediator.destroy();

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());

        // Run the callback (it should be cancelled and not proceed)
        runnableCaptor.getValue().run();

        // Verify that properties were not bound since mediator was destroyed
        assertNull(mPropertyModel.get(BookmarkPopupProperties.TITLE));
    }

    @Test
    public void testOnRemoveClicked() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Edit.Outcome",
                                BookmarkDesktopPopupOutcome.REMOVED)
                        .build();

        mMediator.show(mBookmarkId, false);

        Runnable removeClickListener =
                mPropertyModel.get(BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER);
        removeClickListener.run();

        verify(mBookmarkModel).deleteBookmark(mBookmarkId);
        verify(mDismissRunnable).run();

        mMediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDoneClicked_Edit() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Edit.Outcome",
                                BookmarkDesktopPopupOutcome.SAVED)
                        .build();

        mMediator.show(mBookmarkId, false);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        mPropertyModel.get(BookmarkPopupProperties.TITLE_CHANGED_LISTENER).onResult("New Title");

        Runnable doneClickListener =
                mPropertyModel.get(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER);
        doneClickListener.run();

        verify(mBookmarkModel).setBookmarkTitle(mBookmarkId, "New Title");
        verify(mDismissRunnable).run();

        mMediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDoneClicked_Add() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Add.Outcome",
                                BookmarkDesktopPopupOutcome.SAVED)
                        .build();

        mMediator.show(mBookmarkId, true);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        Runnable doneClickListener =
                mPropertyModel.get(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER);
        doneClickListener.run();

        verify(mDismissRunnable).run();

        mMediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnFolderRowClicked() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Add.Outcome",
                                BookmarkDesktopPopupOutcome.EDIT_DIALOG_OPENED)
                        .build();

        mMediator.show(mBookmarkId, true);

        Runnable folderRowClickListener =
                mPropertyModel.get(BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER);
        folderRowClickListener.run();

        verify(mBookmarkManagerOpener).startEditActivity(mActivity, mProfile, mBookmarkId);
        verify(mDismissRunnable).run();

        mMediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnCloseClicked() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Edit.Outcome",
                                BookmarkDesktopPopupOutcome.DISMISSED)
                        .build();

        mMediator.show(mBookmarkId, false);

        Runnable closeClickListener =
                mPropertyModel.get(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER);
        closeClickListener.run();

        verify(mDismissRunnable).run();

        mMediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShow_WithShoppingSpecifics() {
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder().build())
                        .build();
        when(mBookmarkModel.getPowerBookmarkMeta(mBookmarkId)).thenReturn(meta);

        doAnswer(
                        invocation -> {
                            Callback<Boolean> cb = invocation.getArgument(2);
                            cb.onResult(true);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(Mockito.any(), Mockito.anyLong(), Mockito.any());
        mMediator.show(mBookmarkId, true);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        assertTrue(mPropertyModel.get(BookmarkPopupProperties.PRICE_TRACKING_VISIBLE));
        assertTrue(mPropertyModel.get(BookmarkPopupProperties.PRICE_TRACKING_ENABLED));
    }

    @Test
    public void testShow_NullShoppingService() {
        mMediator =
                new BookmarkPopupMediator(
                        mPropertyModel,
                        mBookmarkModel,
                        mBookmarkManagerOpener,
                        mBookmarkImageFetcher,
                        mActivity,
                        mProfile,
                        null,
                        mPriceDropNotificationManager,
                        mDismissRunnable);

        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder().build())
                        .build();
        when(mBookmarkModel.getPowerBookmarkMeta(mBookmarkId)).thenReturn(meta);

        doAnswer(
                        invocation -> {
                            Callback<Boolean> cb = invocation.getArgument(2);
                            cb.onResult(true);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(Mockito.any(), Mockito.anyLong(), Mockito.any());
        mMediator.show(mBookmarkId, true);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mBookmarkModel).finishLoadingBookmarkModel(runnableCaptor.capture());
        runnableCaptor.getValue().run();

        // Visible would normally be true, but because shopping service is null, it should return
        // early
        assertFalse(mPropertyModel.get(BookmarkPopupProperties.PRICE_TRACKING_VISIBLE));
    }
}
