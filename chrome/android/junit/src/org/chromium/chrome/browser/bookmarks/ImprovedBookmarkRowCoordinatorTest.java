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
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.util.Pair;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
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
public class ImprovedBookmarkRowCoordinatorTest {
    private static final int CHILD_COUNT = 5;
    private static final int READING_LIST_CHILD_COUNT = 1;
    private static final GURL EXAMPLE_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final String EXAMPLE_URL_FORMATTED = UrlFormatter.formatUrlForSecurityDisplay(
            EXAMPLE_URL, SchemeDisplay.OMIT_HTTP_AND_HTTPS);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public final JniMocker mJniMocker = new JniMocker();

    private final BookmarkId mFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/2, BookmarkType.READING_LIST);
    private final BookmarkId mBookmarkId = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final BookmarkId mReadingListId = new BookmarkId(/*id=*/4, BookmarkType.READING_LIST);

    private final BookmarkItem mFolderItem =
            new BookmarkItem(mFolderId, "User folder", null, true, null, true, false, 0, false, 0);
    private final BookmarkItem mBookmarkItem = new BookmarkItem(
            mBookmarkId, "Bookmark", EXAMPLE_URL, false, mFolderId, true, false, 0, false, 0);
    private final BookmarkItem mReadingListFolderItem = new BookmarkItem(
            mReadingListFolderId, "Reading List", null, true, null, true, false, 0, false, 0);
    private final BookmarkItem mReadingListItem = new BookmarkItem(mReadingListId, "ReadingList",
            EXAMPLE_URL, false, mReadingListFolderId, true, false, 0, false, 0);

    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Drawable mDrawable;
    @Mock
    private Runnable mClickListener;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock
    private ShoppingService mShoppingService;
    @Mock
    private CurrencyFormatter.Natives mCurrencyFormatterJniMock;

    private Activity mActivity;
    private ImprovedBookmarkRow mImprovedBookmarkRow;
    private PropertyModel mModel;
    private ImprovedBookmarkRowCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        // Setup BookmarkModel.
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(mReadingListFolderItem).when(mBookmarkModel).getBookmarkById(mReadingListFolderId);
        doReturn(mFolderItem).when(mBookmarkModel).getBookmarkById(mFolderId);
        doReturn(mReadingListItem).when(mBookmarkModel).getBookmarkById(mReadingListId);
        doReturn(CHILD_COUNT).when(mBookmarkModel).getChildCount(mFolderId);
        doReturn(READING_LIST_CHILD_COUNT).when(mBookmarkModel).getChildCount(mReadingListFolderId);

        // Setup BookmarkImageFetcher.
        doCallback(1,
                (Callback<Pair<Drawable, Drawable>> callback)
                        -> callback.onResult(new Pair<>(mDrawable, mDrawable)))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());
        doCallback(1, (Callback<Drawable> callback) -> callback.onResult(mDrawable))
                .when(mBookmarkImageFetcher)
                .fetchImageForBookmarkWithFaviconFallback(any(), any());
        doCallback(1, (Callback<Drawable> callback) -> callback.onResult(mDrawable))
                .when(mBookmarkImageFetcher)
                .fetchFaviconForBookmark(any(), any());

        // Setup CurrencyFormatter.
        mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, mCurrencyFormatterJniMock);

        mCoordinator = new ImprovedBookmarkRowCoordinator(mActivity, mBookmarkImageFetcher,
                mBookmarkModel, mBookmarkUiPrefs, mShoppingService);
    }

    @Test
    public void testFolder_visual() {
        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(mFolderId);

        assertEquals("User folder", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertNull(model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertEquals(String.format("%s %s", model.get(ImprovedBookmarkRowProperties.TITLE),
                             "No bookmarks"),
                model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.SELECTED));
        assertFalse(model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertTrue(model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNull(model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
    }

    @Test
    public void testFolder_compact() {
        doReturn(BookmarkRowDisplayPref.COMPACT).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(mFolderId);

        assertEquals("User folder (0)", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertNull(model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR));
        assertEquals("User folder No bookmarks",
                model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testBookmark_visal() {
        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(mBookmarkId);

        assertEquals("Bookmark", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(EXAMPLE_URL_FORMATTED, model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNull(model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR));
        assertEquals(mDrawable, model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNull(model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testBookmark_visal_nullOutImageWhenBound() {
        doCallback(1, (Callback<Pair<Drawable, Drawable>> callback) -> {})
                .when(mBookmarkImageFetcher)
                .fetchImageForBookmarkWithFaviconFallback(any(), any());

        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(mBookmarkId);

        assertEquals("Bookmark", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(EXAMPLE_URL_FORMATTED, model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNull(model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
    }

    @Test
    public void testBookmark_compact() {
        doReturn(BookmarkRowDisplayPref.COMPACT).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(mBookmarkId);

        assertEquals("Bookmark", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(EXAMPLE_URL_FORMATTED, model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNull(model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR));
    }

    @Test
    public void testBookmark_compact_nullOutImageWhenBoun() {
        doCallback(1, (Callback<Pair<Drawable, Drawable>> callback) -> {})
                .when(mBookmarkImageFetcher)
                .fetchFaviconForBookmark(any(), any());

        doReturn(BookmarkRowDisplayPref.COMPACT).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        PropertyModel model = mCoordinator.createBasePropertyModel(mBookmarkId);

        assertEquals("Bookmark", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(EXAMPLE_URL_FORMATTED, model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNull(model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
    }

    @Test
    public void testShoppingCoordinator() {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);

        ShoppingSpecifics specifics = ShoppingSpecifics.newBuilder().setProductClusterId(1).build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        doReturn(true)
                .when(mShoppingService)
                .isSubscribedFromCache(
                        PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                                specifics));
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(mBookmarkId);

        PropertyModel model = mCoordinator.createBasePropertyModel(mBookmarkId);
        assertNotNull(model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
    }

    @Test
    public void testShoppingCoordinator_nullWhenShoppingListNotEligible() {
        ShoppingFeatures.setShoppingListEligibleForTesting(false);

        ShoppingSpecifics specifics = ShoppingSpecifics.newBuilder().setProductClusterId(1).build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        doReturn(true)
                .when(mShoppingService)
                .isSubscribedFromCache(
                        PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                                specifics));
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(mBookmarkId);

        PropertyModel model = mCoordinator.createBasePropertyModel(mBookmarkId);
        assertNull(model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
    }
}
