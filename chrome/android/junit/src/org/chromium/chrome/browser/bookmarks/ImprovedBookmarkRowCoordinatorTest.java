// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ImprovedBookmarkRowCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE})
public class ImprovedBookmarkRowCoordinatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Drawable mDrawable;
    @Mock private Drawable mFavicon;
    @Mock private Runnable mClickListener;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private ShoppingService mShoppingService;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private CurrencyFormatter.Natives mCurrencyFormatterJniMock;
    @Mock private ImprovedBookmarkRow mImprovedBookmarkRow;

    private Activity mActivity;
    private PropertyModel mModel;
    private ImprovedBookmarkRowCoordinator mCoordinator;

    @Before
    public void setUp() {
        doReturn(false).when(mBookmarkModel).areAccountBookmarkFoldersActive();
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        // Setup BookmarkModel.
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);

        // Setup BookmarkImageFetcher.
        doCallback(
                        1,
                        (Callback<Pair<Drawable, Drawable>> callback) ->
                                callback.onResult(new Pair<>(mDrawable, mDrawable)))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());
        setBookmarkImageReturnValue(mDrawable);
        doCallback(1, (Callback<Drawable> callback) -> callback.onResult(mFavicon))
                .when(mBookmarkImageFetcher)
                .fetchFaviconForBookmark(any(), any());

        // Setup CurrencyFormatter.
        mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, mCurrencyFormatterJniMock);

        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);

        mCoordinator =
                new ImprovedBookmarkRowCoordinator(
                        mActivity,
                        mBookmarkImageFetcher,
                        mBookmarkModel,
                        mBookmarkUiPrefs,
                        mShoppingService);
    }

    @Test
    public void testBookmark_visal() {
        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(URL_BOOKMARK_ID_A);

        assertEquals("Url A", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(
                UrlFormatter.formatUrlForSecurityDisplay(
                        JUnitTestGURLs.RED_1, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertEquals(mDrawable, model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE).get());
        assertNull(model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testBookmark_compact() {
        doReturn(BookmarkRowDisplayPref.COMPACT).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(URL_BOOKMARK_ID_A);

        assertEquals("Url A", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(
                UrlFormatter.formatUrlForSecurityDisplay(
                        JUnitTestGURLs.RED_1, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
    }

    @Test
    public void testShoppingCoordinator() {
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        ShoppingSpecifics specifics = ShoppingSpecifics.newBuilder().setProductClusterId(1).build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        doReturn(true)
                .when(mShoppingService)
                .isSubscribedFromCache(
                        PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                                specifics));
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_A);

        PropertyModel model = mCoordinator.createBasePropertyModel(URL_BOOKMARK_ID_A);
        assertNotNull(model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
    }

    @Test
    public void testShoppingCoordinator_nullWhenShoppingListNotEligible() {
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        ShoppingSpecifics specifics = ShoppingSpecifics.newBuilder().setProductClusterId(1).build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        doReturn(true)
                .when(mShoppingService)
                .isSubscribedFromCache(
                        PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                                specifics));
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_A);

        PropertyModel model = mCoordinator.createBasePropertyModel(URL_BOOKMARK_ID_A);
        assertNull(model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
    }

    @Test
    public void testFolder_compact() {
        doReturn(BookmarkRowDisplayPref.COMPACT).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(FOLDER_BOOKMARK_ID_A);

        assertEquals("Folder A (0)", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(
                "Folder A No bookmarks",
                model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testFolder_local() {
        FakeBookmarkModel bookmarkModel = FakeBookmarkModel.createModel();
        bookmarkModel.setAreAccountBookmarkFoldersActive(true);

        mCoordinator =
                new ImprovedBookmarkRowCoordinator(
                        mActivity,
                        mBookmarkImageFetcher,
                        bookmarkModel,
                        mBookmarkUiPrefs,
                        mShoppingService);
        PropertyModel model =
                mCoordinator.createBasePropertyModel(bookmarkModel.getMobileFolderId());
        assertEquals(
                "Mobile bookmarks 1 bookmark Only on this device",
                model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testFolder_compactConversionString() {
        // Need to be careful when formatting user generating content, https://crbug.com/1509959.
        BookmarkId folderId = new BookmarkId(100, BookmarkType.NORMAL);
        BookmarkItem folder =
                new BookmarkItem(
                        folderId,
                        "Folder %M %d %s",
                        null,
                        true,
                        MOBILE_BOOKMARK_ID,
                        true,
                        false,
                        0,
                        false,
                        0,
                        false);
        when(mBookmarkModel.getBookmarkById(folderId)).thenReturn(folder);
        PropertyModel model = mCoordinator.createBasePropertyModel(folderId);

        assertEquals("Folder %M %d %s (0)", model.get(ImprovedBookmarkRowProperties.TITLE));
    }

    @Test
    public void testFolder_visual() {
        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(FOLDER_BOOKMARK_ID_A);

        assertEquals("Folder A", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertNull(model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertEquals(
                String.format(
                        "%s %s", model.get(ImprovedBookmarkRowProperties.TITLE), "No bookmarks"),
                model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.SELECTED));
        assertFalse(model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertTrue(model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNull(model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
        assertEquals(0, model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));
        assertEquals(
                new Pair<>(mDrawable, mDrawable),
                model.get(ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES).get());
    }

    @Test
    public void testVisualFolder_readingList() {
        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(READING_LIST_BOOKMARK_ID);
        assertFalse(mCoordinator.shouldShowImagesForFolder(READING_LIST_BOOKMARK_ID));

        assertNotNull(model.get(ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE));
        assertEquals(0, model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE));
        assertEquals(
                new Pair<Drawable, Drawable>(null, null),
                model.get(ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES).get());
    }

    @Test
    @EnableFeatures({SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE})
    public void testBookmark_accountAndLocal() throws Exception {
        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        FakeBookmarkModel bookmarkModel = FakeBookmarkModel.createModel();
        bookmarkModel.setAreAccountBookmarkFoldersActive(true);
        mCoordinator =
                new ImprovedBookmarkRowCoordinator(
                        mActivity,
                        mBookmarkImageFetcher,
                        bookmarkModel,
                        mBookmarkUiPrefs,
                        mShoppingService);
        BookmarkId localBookmarkId =
                bookmarkModel.addBookmark(
                        bookmarkModel.getMobileFolderId(),
                        0,
                        "local",
                        new GURL("https://local.com/"));
        BookmarkId accountBookmarkId =
                bookmarkModel.addToReadingList(
                        bookmarkModel.getAccountReadingListFolder(),
                        "account",
                        new GURL("https://account.com/"));

        setBookmarkImageReturnValue(mFavicon);
        PropertyModel model = mCoordinator.createBasePropertyModel(localBookmarkId);
        assertTrue(model.get(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK));
        // Local bookmarks will still go through the native consent flow.
        assertTrue(
                mCoordinator.shouldShowImagesForBookmark(
                        bookmarkModel.getBookmarkById(localBookmarkId),
                        BookmarkRowDisplayPref.VISUAL));
        assertEquals(mFavicon, model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE).get());

        setBookmarkImageReturnValue(mDrawable);
        model = mCoordinator.createBasePropertyModel(bookmarkModel.getMobileFolderId());
        assertTrue(model.get(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK));
        assertEquals(
                new Pair<>(null, null),
                model.get(ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES).get());
        assertEquals(2, model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));

        model = mCoordinator.createBasePropertyModel(accountBookmarkId);
        assertFalse(model.get(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK));
    }

    private void setBookmarkImageReturnValue(@Nullable Drawable drawable) {
        doCallback(1, (Callback<Drawable> callback) -> callback.onResult(drawable))
                .when(mBookmarkImageFetcher)
                .fetchImageForBookmarkWithFaviconFallback(any(), any());
    }
}
