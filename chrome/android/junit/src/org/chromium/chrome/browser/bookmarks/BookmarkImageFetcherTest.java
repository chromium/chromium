// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta.Image;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Unit tests for {@link BookmarkImageFetcher}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkImageFetcherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private RoundedIconGenerator mIconGenerator;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private Callback<Drawable> mDrawableCallback;
    @Mock private Callback<Pair<Drawable, Drawable>> mFolderDrawablesCallback;
    @Mock private Profile mProfile;
    @Mock private ImageServiceBridgeJni mImageServiceBridgeJni;
    @Mock private ImageServiceBridge mImageServiceBridge;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private DefaultFaviconHelper mDefaultFaviconHelper;

    @Captor private ArgumentCaptor<Drawable> mDrawableCaptor;
    @Captor private ArgumentCaptor<Pair<Drawable, Drawable>> mFolderDrawablesCaptor;
    @Captor private ArgumentCaptor<Callback<GURL>> mGURLCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private final BookmarkId mFolderId = new BookmarkId(/* id= */ 1, BookmarkType.NORMAL);
    private final BookmarkId mBookmarkId1 = new BookmarkId(/* id= */ 2, BookmarkType.NORMAL);
    private final BookmarkId mBookmarkId2 = new BookmarkId(/* id= */ 3, BookmarkType.NORMAL);

    private final BookmarkItem mFolderItem =
            new BookmarkItem(
                    mFolderId, "Folder", null, true, null, true, false, 0, false, 0, false);
    private final BookmarkItem mBookmarkItem1 =
            new BookmarkItem(
                    mBookmarkId1,
                    "Bookmark1",
                    JUnitTestGURLs.EXAMPLE_URL,
                    false,
                    mFolderId,
                    true,
                    false,
                    0,
                    false,
                    0,
                    false);
    private final BookmarkItem mBookmarkItem2 =
            new BookmarkItem(
                    mBookmarkId2,
                    "Bookmark1",
                    JUnitTestGURLs.EXAMPLE_URL,
                    false,
                    mFolderId,
                    true,
                    false,
                    0,
                    false,
                    0,
                    false);
    private final Bitmap mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);

    private Activity mActivity;
    private BookmarkImageFetcher mBookmarkImageFetcher;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = spy(activity);

                            // Setup BookmarkModel.
                            doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
                            doReturn(Arrays.asList(mBookmarkId1, mBookmarkId2))
                                    .when(mBookmarkModel)
                                    .getChildIds(mFolderId);
                            doReturn(mFolderItem).when(mBookmarkModel).getBookmarkById(mFolderId);
                            doReturn(mBookmarkItem1)
                                    .when(mBookmarkModel)
                                    .getBookmarkById(mBookmarkId1);
                            doReturn(mBookmarkItem2)
                                    .when(mBookmarkModel)
                                    .getBookmarkById(mBookmarkId2);

                            // Setup image fetching.
                            doCallback(
                                            1,
                                            (Callback<Bitmap> callback) -> {
                                                callback.onResult(mBitmap);
                                            })
                                    .when(mImageFetcher)
                                    .fetchImage(any(), any());
                            doCallback(
                                            3,
                                            (Callback<Bitmap> callback) -> {
                                                callback.onResult(mBitmap);
                                            })
                                    .when(mImageServiceBridge)
                                    .fetchImageFor(anyBoolean(), any(), anyInt(), any());
                            doCallback(
                                            3,
                                            (FaviconImageCallback callback) ->
                                                    callback.onFaviconAvailable(mBitmap, null))
                                    .when(mFaviconHelper)
                                    .getForeignFaviconImageForURL(any(), any(), anyInt(), any());
                            doCallback(
                                            3,
                                            (FaviconImageCallback callback) ->
                                                    callback.onFaviconAvailable(mBitmap, null))
                                    .when(mFaviconHelper)
                                    .getLocalFaviconImageForURL(any(), any(), anyInt(), any());
                            doReturn(true)
                                    .when(mImageServiceBridge)
                                    .hasConsentToFetchImages(anyBoolean());

                            mBookmarkImageFetcher =
                                    new BookmarkImageFetcher(
                                            mProfile,
                                            mActivity,
                                            mBookmarkModel,
                                            mImageFetcher,
                                            mIconGenerator,
                                            mImageServiceBridge,
                                            mFaviconHelper,
                                            mDefaultFaviconHelper);
                        });
    }

    @Test
    public void testFetchFirstTwoImagesForFolder() {
        mBookmarkImageFetcher.fetchFirstTwoImagesForFolder(
                mFolderItem, 100, mFolderDrawablesCallback);
        verify(mFolderDrawablesCallback).onResult(mFolderDrawablesCaptor.capture());

        Pair<Drawable, Drawable> drawables = mFolderDrawablesCaptor.getValue();
        assertNotNull(drawables.first);
        assertNotNull(drawables.second);
    }

    @Test
    public void testFetchFirstTwoImagesForFolder_nullChild() {
        doReturn(null).when(mBookmarkModel).getBookmarkById(mBookmarkId2);

        mBookmarkImageFetcher.fetchFirstTwoImagesForFolder(
                mFolderItem, 100, mFolderDrawablesCallback);
        verify(mFolderDrawablesCallback).onResult(mFolderDrawablesCaptor.capture());

        Pair<Drawable, Drawable> drawables = mFolderDrawablesCaptor.getValue();
        assertNotNull(drawables.first);
        assertNull(drawables.second);
    }

    @Test
    public void testFetchImageForBookmarkWithFaviconFallback() {
        mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(
                mBookmarkItem1, 100, mDrawableCallback);
        verify(mDrawableCallback).onResult(mDrawableCaptor.capture());
        // There shouldn't be any interaction with favicon helper since an image was found.
        verify(mFaviconHelper, times(0))
                .getForeignFaviconImageForURL(any(), any(), anyInt(), any());

        assertNotNull(mDrawableCaptor.getValue());
    }

    @Test
    public void testFetchImageForBookmarkWithFaviconFallback_withShoppingSpecifics() {
        String metaImageUrl = "https://shoppingmeta.com/";
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build())
                        .setLeadImage(Image.newBuilder().setUrl(metaImageUrl).build())
                        .build();

        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(mBookmarkId1);
        mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(
                mBookmarkItem1, 100, mDrawableCallback);

        ArgumentCaptor<ImageFetcher.Params> paramsArgumentCaptor =
                ArgumentCaptor.forClass(ImageFetcher.Params.class);
        verify(mImageFetcher).fetchImage(paramsArgumentCaptor.capture(), any());
        assertEquals(paramsArgumentCaptor.getValue().url, metaImageUrl);

        verify(mDrawableCallback).onResult(mDrawableCaptor.capture());
        // There shouldn't be any interaction with favicon helper since an image was found.
        verify(mFaviconHelper, times(0))
                .getForeignFaviconImageForURL(any(), any(), anyInt(), any());

        assertNotNull(mDrawableCaptor.getValue());
    }

    @Test
    public void testFetchImageForBookmarkWithFaviconFallback_fallbackToFavicon() {
        doCallback(
                        3,
                        (Callback<Bitmap> callback) -> {
                            callback.onResult(null);
                        })
                .when(mImageServiceBridge)
                .fetchImageFor(anyBoolean(), any(), anyInt(), any());

        mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(
                mBookmarkItem1, 100, mDrawableCallback);
        verify(mDrawableCallback).onResult(mDrawableCaptor.capture());
        verify(mFaviconHelper).getForeignFaviconImageForURL(any(), any(), anyInt(), any());

        assertNotNull(mDrawableCaptor.getValue());
    }

    @Test
    public void testFetchFaviconForBookmark_Foreign() {
        doReturn(true).when(mImageServiceBridge).hasConsentToFetchImages(anyBoolean());

        mBookmarkImageFetcher.fetchFaviconForBookmark(mBookmarkItem1, mDrawableCallback);
        verify(mDrawableCallback).onResult(mDrawableCaptor.capture());
        verify(mFaviconHelper).getForeignFaviconImageForURL(any(), any(), anyInt(), any());

        assertNotNull(mDrawableCaptor.getValue());
    }

    @Test
    public void testFetchFaviconForBookmark_Local() {
        doReturn(false).when(mImageServiceBridge).hasConsentToFetchImages(anyBoolean());

        mBookmarkImageFetcher.fetchFaviconForBookmark(mBookmarkItem1, mDrawableCallback);
        verify(mDrawableCallback).onResult(mDrawableCaptor.capture());
        verify(mFaviconHelper).getLocalFaviconImageForURL(any(), any(), anyInt(), any());

        assertNotNull(mDrawableCaptor.getValue());
    }
}
