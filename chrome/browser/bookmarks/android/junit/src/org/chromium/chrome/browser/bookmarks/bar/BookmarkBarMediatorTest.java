// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Point;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuGesture;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarItemsProvider.ObservationId;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ClickWithMetaStateCallback;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.BOOKMARKS_BAR_NTP,
    ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT,
    ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG
})
public class BookmarkBarMediatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModel mPropertyModel;
    @Mock private ModelList mItemsModel;
    @Mock private PropertyModel mAllBookmarksButtonModel;
    @Mock private Profile mProfile;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private RecyclerView mItemsRecyclerView;
    @Mock private BookmarkBar mBookmarkBarView;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private BookmarkBarItemsLayoutManager mLayoutManager;
    @Mock private BookmarkBarPopupCoordinator mPopupCoordinator;
    @Mock private Tab mTab;
    @Mock private UserPrefsJni mUserPrefsJni;
    @Mock private PrefService mPrefService;
    @Mock private ImageServiceBridgeJni mImageServiceBridgeJni;
    @Mock private FaviconHelperJni mFaviconHelperJni;

    private final SettableNonNullObservableSupplier<Boolean> mItemsOverflowSupplier =
            ObservableSuppliers.createNonNull(false);
    private Activity mActivity;
    private BookmarkBarMediator mMediator;
    private FakeBookmarkModel mBookmarkModel;
    private SettableNonNullObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        doReturn(1L).when(mFaviconHelperJni).init();
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeJni);
        mProfileSupplier = ObservableSuppliers.createNonNull(mProfile);
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkModel = FakeBookmarkModel.createModel();
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        Supplier<Pair<Integer, Integer>> controlsHeightSupplier = () -> new Pair<>(0, 0);
        when(mLayoutManager.getItemsOverflowSupplier()).thenReturn(mItemsOverflowSupplier);

        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        mMediator =
                new BookmarkBarMediator(
                        mActivity,
                        mAllBookmarksButtonModel,
                        mItemsModel,
                        mLayoutManager,
                        mPropertyModel,
                        mProfileSupplier,
                        /* currentTabSupplier= */ () -> mTab,
                        mBookmarkOpener,
                        ObservableSuppliers.createNonNull(mBookmarkManagerOpener),
                        () -> mSnackbarManager,
                        () -> mModalDialogManager,
                        mItemsRecyclerView,
                        mBookmarkBarView,
                        mPopupCoordinator,
                        ObservableSuppliers.createNonNull(false));
    }

    @After
    public void tearDown() throws Exception {
        UserPrefsJni.setInstanceForTesting(null);
        mMediator.destroy();
        assertNull(mMediator.getFolderIconBitmapForTesting());
    }

    // Tests the behavior of mFolderIconBitmap.
    @Test
    @SmallTest
    public void testFolderIconBitmap() throws Exception {
        // Create a new folder inside the bookmarks bar.
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        BookmarkId rootFolderId = mBookmarkModel.addFolder(desktopFolderId, 0, "Root Folder");

        // Add a child folder to the root folder to ensure the caching logic is triggered.
        mBookmarkModel.addFolder(rootFolderId, 0, "Child Folder");

        assertNull("Cache should be empty initially.", mMediator.getFolderIconBitmapForTesting());

        // Trigger #createListItemForBookmarkFolder, which populates the mFolderIconBitmap cache.
        mMediator.buildMenuModelListForFolder(mBookmarkModel, rootFolderId);

        assertNotNull("Cache should be populated.", mMediator.getFolderIconBitmapForTesting());

        // Destroy behavior is tested in #tearDown.
    }

    @Test
    @SmallTest
    public void testBuildMenuModelListForFolder_createsCorrectStructure() {
        // Setup a nested folder structure: F1 -> (L1, F2 -> L2)
        BookmarkId f1 = mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "F1");
        mBookmarkModel.addBookmark(f1, 0, "L1", JUnitTestGURLs.URL_1);
        BookmarkId f2 = mBookmarkModel.addFolder(f1, 1, "F2");
        mBookmarkModel.addBookmark(f2, 0, "L2", JUnitTestGURLs.URL_2);

        // Build the model list for the top-level folder F1.
        ModelList modelList = mMediator.buildMenuModelListForFolder(mBookmarkModel, f1);

        // Verify the structure of the top-level menu.
        assertEquals("Top-level menu should have two items (L1, F2).", 2, modelList.size());

        // Verify the first item (L1).
        ListItem l1ListItem = modelList.get(0);
        assertEquals(ListItemType.MENU_ITEM, l1ListItem.type);
        assertEquals("L1", l1ListItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                Resources.ID_NULL,
                l1ListItem.model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));

        // Verify the second item (F2), which should be a submenu.
        ListItem f2ListItem = modelList.get(1);
        assertEquals(ListItemType.MENU_ITEM_WITH_SUBMENU, f2ListItem.type);
        assertEquals("F2", f2ListItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.color.default_icon_color_tint_list,
                f2ListItem.model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));

        // Verify the structure of the submenu.
        List<ListItem> submenuItems =
                f2ListItem.model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER).get();
        assertNotNull("Submenu items list should not be null.", submenuItems);
        assertEquals("Submenu should have one item (L2).", 1, submenuItems.size());

        // Verify the item in the submenu (L2).
        ListItem l2ListItem = submenuItems.get(0);
        assertEquals(ListItemType.MENU_ITEM, l2ListItem.type);
        assertEquals("L2", l2ListItem.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testBuildMenuModelListFromIds_showsOnlyHiddenItems() {
        // Create 5 bookmarks in the desktop folder.
        BookmarkId desktopFolder = mBookmarkModel.getDesktopFolderId();
        mBookmarkModel.addBookmark(desktopFolder, 0, "Visible 1", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(desktopFolder, 1, "Visible 2", JUnitTestGURLs.URL_2);
        mBookmarkModel.addBookmark(desktopFolder, 2, "Hidden 1", JUnitTestGURLs.URL_3);
        mBookmarkModel.addBookmark(desktopFolder, 3, "Hidden 2", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(desktopFolder, 4, "Hidden 3", JUnitTestGURLs.URL_2);

        // Get the full list of all 5 bookmark IDs.
        List<BookmarkId> allItemIds = mBookmarkModel.getChildIds(desktopFolder);

        when(mLayoutManager.getFirstHiddenItemPosition()).thenReturn(2);

        // The first 2 items are visible, so the hidden items start at index 2.
        int firstHiddenIndex = mLayoutManager.getFirstHiddenItemPosition();
        List<BookmarkId> hiddenItemIds = allItemIds.subList(firstHiddenIndex, allItemIds.size());

        // Call #buildMenuModelListFromIds with the list of hidden item IDs.
        ModelList hiddenItemsModel =
                mMediator.buildMenuModelListFromIds(mBookmarkModel, hiddenItemIds);

        assertEquals("The model should only contain the hidden items.", 3, hiddenItemsModel.size());
        assertEquals(
                "The first item in the model should be the first hidden bookmark.",
                "Hidden 1",
                hiddenItemsModel.get(0).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "The last item in the model should be the last hidden bookmark.",
                "Hidden 3",
                hiddenItemsModel.get(2).model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @SmallTest
    public void testOnThemeChanged_UpdatesAllBookmarksButton() {
        // Stub the iterator on the mock mItemsModel to prevent a NullPointerException.
        // This test doesn't care about the model list, only the "All Bookmarks" button.
        when(mItemsModel.iterator()).thenReturn(new ArrayList<ListItem>().iterator());

        // Call onThemeChanged for Incognito.
        mMediator.onThemeChanged(/* isIncognito= */ true, BrandedColorScheme.INCOGNITO);

        // Verify the "All Bookmarks" button model was updated for incognito.
        verify(mAllBookmarksButtonModel)
                .set(
                        BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                        R.color.default_icon_color_light_tint_list);
        verify(mAllBookmarksButtonModel)
                .set(
                        BookmarkBarButtonProperties.TEXT_APPEARANCE_ID,
                        R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light);
        verify(mAllBookmarksButtonModel)
                .set(
                        BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID,
                        R.drawable.bookmark_bar_ripple_baseline);
    }

    @Test
    @SmallTest
    public void testOnThemeChanged_ThemeChangedFirstAndThenAnItemIsAdded() {
        // Since we are adding an item after #onThemeChanged is called, the for-loop inside
        // #onThemeChanged will be skipped.
        when(mItemsModel.iterator()).thenReturn(new ArrayList<ListItem>().iterator());

        // 1. Call onThemeChanged for Incognito. This saves the light theme to the Mediator's state
        // variables.
        mMediator.onThemeChanged(/* isIncognito= */ true, BrandedColorScheme.INCOGNITO);

        // 2. Create a new folder inside the bookmarks bar.
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");

        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        // 3. This will trigger #createListItemFor, which should now use the light theme saved in
        // 1., and update the colors. In production, this line will be auomatically called after
        // #onThemeChanged but we are calling it manually in the unit test.
        mMediator.onBookmarkItemAdded(ObservationId.LOCAL, folderItem, 0);

        // Verify that mItemsModel.add() was called, and save the ListItem that was passed.
        ArgumentCaptor<ListItem> listItemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mItemsModel).add(eq(0), listItemCaptor.capture());
        PropertyModel itemModel = listItemCaptor.getValue().model;

        assertEquals(
                "New item should have the light text style",
                R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light,
                itemModel.get(BookmarkBarButtonProperties.TEXT_APPEARANCE_ID));

        assertEquals(
                "New folder item should have the light icon tint",
                R.color.default_icon_color_light_tint_list,
                itemModel.get(BookmarkBarButtonProperties.ICON_TINT_LIST_ID));

        assertEquals(
                "New item should have the baseline background",
                R.drawable.bookmark_bar_ripple_baseline,
                itemModel.get(BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID));
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemClick_MiddleClick() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        mMediator.onBookmarkItemAdded(ObservationId.LOCAL, bookmarkItem, 0);

        ArgumentCaptor<ListItem> listItemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mItemsModel).add(eq(0), listItemCaptor.capture());
        PropertyModel itemModel = listItemCaptor.getValue().model;

        ClickWithMetaStateCallback clickCallback =
                itemModel.get(BookmarkBarButtonProperties.CLICK_CALLBACK);
        assertNotNull(clickCallback);

        // Simulate middle click
        clickCallback.onClickWithMeta(0, MotionEvent.BUTTON_TERTIARY);

        verify(mBookmarkOpener)
                .openBookmarksInNewTabs(
                        eq(List.of(bookmarkId)),
                        eq(false),
                        eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testPopupMenuItemTouchListener_MiddleClickConsumed() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        OnTouchListener touchListener = listItem.model.get(ListMenuItemProperties.TOUCH_LISTENER);

        View placeholderView = new View(mActivity);

        // Simulate ACTION_DOWN
        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        assertTrue(touchListener.onTouch(placeholderView, downEvent));

        // Simulate ACTION_BUTTON_RELEASE with BUTTON_TERTIARY
        MotionEvent releaseEvent = mock(MotionEvent.class);
        when(releaseEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(releaseEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        assertTrue(touchListener.onTouch(placeholderView, releaseEvent));

        verifyNoInteractions(mBookmarkOpener);
    }

    @Test
    @SmallTest
    public void testPopupMenuItemGenericMotion_MiddleClick() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        View.OnGenericMotionListener listener =
                listItem.model.get(ListMenuItemProperties.GENERIC_MOTION_LISTENER);
        assertNotNull(listener);

        View placeholderView = new View(mActivity);
        MotionEvent releaseEvent = mock(MotionEvent.class);
        when(releaseEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(releaseEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        assertTrue(listener.onGenericMotion(placeholderView, releaseEvent));

        verify(mBookmarkOpener)
                .openBookmarksInNewTabs(
                        eq(List.of(bookmarkId)),
                        eq(false),
                        eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testPopupMenuItemClickListener_CtrlClick_Url() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);

        // Simulate Ctrl Key active in Touch events to fake state
        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getMetaState()).thenReturn(KeyEvent.META_CTRL_ON);
        listItem.model
                .get(ListMenuItemProperties.TOUCH_LISTENER)
                .onTouch(new View(mActivity), downEvent);

        View.OnClickListener listener = listItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        listener.onClick(new View(mActivity));

        verify(mBookmarkOpener)
                .openBookmarksInNewTabs(
                        eq(List.of(bookmarkId)),
                        eq(false),
                        eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testPopupMenuItemClickListener_CtrlClick_Folder() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        BookmarkId folderId = mBookmarkModel.addFolder(desktopFolderId, 0, "Test Folder");
        BookmarkId urlId1 = mBookmarkModel.addBookmark(folderId, 0, "B1", JUnitTestGURLs.URL_1);
        BookmarkId urlId2 = mBookmarkModel.addBookmark(folderId, 0, "B2", JUnitTestGURLs.URL_2);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0); // Should be the Test Folder

        // Simulate Ctrl Key active
        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getMetaState()).thenReturn(KeyEvent.META_CTRL_ON);
        listItem.model
                .get(ListMenuItemProperties.TOUCH_LISTENER)
                .onTouch(new View(mActivity), downEvent);

        View.OnClickListener listener = listItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        listener.onClick(new View(mActivity));

        // Expect it to bulk-open the children URLs
        verify(mBookmarkOpener)
                .openFolderBookmarksInNewTabs(
                        eq(folderId), eq(false), eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testPopupMenuItemTouchListener_PrimaryClickNotConsumed() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        mBookmarkModel.addBookmark(desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        OnTouchListener touchListener = listItem.model.get(ListMenuItemProperties.TOUCH_LISTENER);

        View placeholderView = new View(mActivity);

        // Simulate ACTION_DOWN with primary button (or touch).
        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_PRIMARY);

        assertFalse(
                "ACTION_DOWN for primary click should not be consumed so tooltips can work",
                touchListener.onTouch(placeholderView, downEvent));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testEmptySpaceRightClick_ContextMenuEnabled() {
        ArgumentCaptor<BookmarkBar.EmptySpaceContextMenuCallback> captor =
                ArgumentCaptor.forClass(BookmarkBar.EmptySpaceContextMenuCallback.class);
        verify(mBookmarkBarView).setEmptySpaceContextMenuCallback(captor.capture());
        BookmarkBar.EmptySpaceContextMenuCallback callback = captor.getValue();
        assertNotNull(callback);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.EmptySpace.RightClick.Opened",
                                true)
                        .build();

        callback.onContextMenuTriggered(100f, 200f, BookmarkBarContextMenuGesture.RIGHT_CLICK);

        verify(mPopupCoordinator)
                .showContextMenuPopup(
                        any(), eq(mBookmarkBarView), eq(new Point(100, 200)), eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testEmptySpaceRightClick_ContextMenuDisabled() {
        ArgumentCaptor<BookmarkBar.EmptySpaceContextMenuCallback> captor =
                ArgumentCaptor.forClass(BookmarkBar.EmptySpaceContextMenuCallback.class);
        verify(mBookmarkBarView).setEmptySpaceContextMenuCallback(captor.capture());
        BookmarkBar.EmptySpaceContextMenuCallback callback = captor.getValue();
        assertNotNull(callback);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Bookmarks.BookmarkBar.ContextMenu.EmptySpace.RightClick.Opened")
                        .build();

        callback.onContextMenuTriggered(100f, 200f, BookmarkBarContextMenuGesture.RIGHT_CLICK);

        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testEmptySpaceLongClick_ContextMenuEnabled() {
        ArgumentCaptor<BookmarkBar.EmptySpaceContextMenuCallback> captor =
                ArgumentCaptor.forClass(BookmarkBar.EmptySpaceContextMenuCallback.class);
        verify(mBookmarkBarView).setEmptySpaceContextMenuCallback(captor.capture());
        BookmarkBar.EmptySpaceContextMenuCallback callback = captor.getValue();
        assertNotNull(callback);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.EmptySpace.LongPress.Opened",
                                true)
                        .build();

        callback.onContextMenuTriggered(100f, 200f, BookmarkBarContextMenuGesture.LONG_PRESS);

        verify(mPopupCoordinator)
                .showContextMenuPopup(
                        any(), eq(mBookmarkBarView), eq(new Point(100, 200)), eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testEmptySpaceLongClick_ContextMenuDisabled() {
        ArgumentCaptor<BookmarkBar.EmptySpaceContextMenuCallback> captor =
                ArgumentCaptor.forClass(BookmarkBar.EmptySpaceContextMenuCallback.class);
        verify(mBookmarkBarView).setEmptySpaceContextMenuCallback(captor.capture());
        BookmarkBar.EmptySpaceContextMenuCallback callback = captor.getValue();
        assertNotNull(callback);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Bookmarks.BookmarkBar.ContextMenu.EmptySpace.LongPress.Opened")
                        .build();

        callback.onContextMenuTriggered(100f, 200f, BookmarkBarContextMenuGesture.LONG_PRESS);

        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testBookmarkItemRightClick_ContextMenuEnabled() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        mMediator.onBookmarkItemAdded(ObservationId.LOCAL, bookmarkItem, 0);

        ArgumentCaptor<ListItem> listItemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mItemsModel).add(eq(0), listItemCaptor.capture());
        PropertyModel itemModel = listItemCaptor.getValue().model;

        ClickWithMetaStateCallback clickCallback =
                itemModel.get(BookmarkBarButtonProperties.CLICK_CALLBACK);
        assertNotNull(clickCallback);

        View mockView = mock(View.class);
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(mockView) {};
        when(mItemsRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(viewHolder);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarItem"
                                        + ".RightClick.Opened",
                                true)
                        .build();

        clickCallback.onClickWithMeta(0, MotionEvent.BUTTON_SECONDARY);

        verify(mPopupCoordinator).showContextMenuPopup(any(), eq(mockView), any(), eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testBookmarkItemRightClick_ContextMenuDisabled() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        mMediator.onBookmarkItemAdded(ObservationId.LOCAL, bookmarkItem, 0);

        ArgumentCaptor<ListItem> listItemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mItemsModel).add(eq(0), listItemCaptor.capture());
        PropertyModel itemModel = listItemCaptor.getValue().model;

        ClickWithMetaStateCallback clickCallback =
                itemModel.get(BookmarkBarButtonProperties.CLICK_CALLBACK);
        assertNotNull(clickCallback);

        View mockView = mock(View.class);
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(mockView) {};
        when(mItemsRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(viewHolder);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarItem"
                                        + ".RightClick.Opened")
                        .build();

        clickCallback.onClickWithMeta(0, MotionEvent.BUTTON_SECONDARY);

        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testBookmarkItemLongClick_ContextMenuEnabled() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        mMediator.onBookmarkItemAdded(ObservationId.LOCAL, bookmarkItem, 0);

        ArgumentCaptor<ListItem> listItemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mItemsModel).add(eq(0), listItemCaptor.capture());
        PropertyModel itemModel = listItemCaptor.getValue().model;

        View.OnLongClickListener longClickListener =
                itemModel.get(BookmarkBarButtonProperties.LONG_CLICK_LISTENER);
        assertNotNull(longClickListener);

        View mockView = mock(View.class);
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(mockView) {};
        when(mItemsRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(viewHolder);

        Callback<Point> pointCallback = itemModel.get(BookmarkBarButtonProperties.POINT_CALLBACK);
        assertNotNull(pointCallback);
        pointCallback.onResult(new Point(10, 20));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarItem"
                                        + ".LongPress.Opened",
                                true)
                        .build();

        assertTrue(longClickListener.onLongClick(mockView));

        verify(mPopupCoordinator)
                .showContextMenuPopup(any(), eq(mockView), eq(new Point(10, 20)), eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testBookmarkItemLongClick_ContextMenuDisabled() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        mMediator.onBookmarkItemAdded(ObservationId.LOCAL, bookmarkItem, 0);

        ArgumentCaptor<ListItem> listItemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mItemsModel).add(eq(0), listItemCaptor.capture());
        PropertyModel itemModel = listItemCaptor.getValue().model;

        View.OnLongClickListener longClickListener =
                itemModel.get(BookmarkBarButtonProperties.LONG_CLICK_LISTENER);
        assertNotNull(longClickListener);

        View mockView = mock(View.class);
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(mockView) {};
        when(mItemsRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(viewHolder);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarItem"
                                        + ".LongPress.Opened")
                        .build();

        assertFalse(longClickListener.onLongClick(mockView));

        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testPopupMenuItemRightClickListener_ContextMenuEnabled() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        mBookmarkModel.addBookmark(desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        View placeholderView = new View(mActivity);

        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_SECONDARY);
        when(downEvent.getX()).thenReturn(50f);
        when(downEvent.getY()).thenReturn(60f);

        OnTouchListener touchListener = listItem.model.get(ListMenuItemProperties.TOUCH_LISTENER);
        assertNotNull(touchListener);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.PopupItem.RightClick.Opened",
                                true)
                        .build();

        assertTrue(touchListener.onTouch(placeholderView, downEvent));

        verify(mPopupCoordinator)
                .showContextMenuPopup(any(), eq(placeholderView), eq(new Point(50, 60)), eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testPopupMenuItemRightClickListener_ContextMenuDisabled() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        mBookmarkModel.addBookmark(desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        View placeholderView = new View(mActivity);

        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_SECONDARY);
        when(downEvent.getX()).thenReturn(50f);
        when(downEvent.getY()).thenReturn(60f);

        OnTouchListener touchListener = listItem.model.get(ListMenuItemProperties.TOUCH_LISTENER);
        assertNotNull(touchListener);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Bookmarks.BookmarkBar.ContextMenu.PopupItem.RightClick.Opened")
                        .build();

        assertFalse(touchListener.onTouch(placeholderView, downEvent));

        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testPopupMenuItemLongClickListener_ContextMenuEnabled() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        mBookmarkModel.addBookmark(desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        View placeholderView = new View(mActivity);

        MotionEvent downEvent = mock(MotionEvent.class);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(downEvent.getX()).thenReturn(50f);
        when(downEvent.getY()).thenReturn(60f);

        OnTouchListener touchListener = listItem.model.get(ListMenuItemProperties.TOUCH_LISTENER);
        assertNotNull(touchListener);
        touchListener.onTouch(placeholderView, downEvent);

        View.OnLongClickListener longClickListener =
                listItem.model.get(ListMenuItemProperties.LONG_CLICK_LISTENER);
        assertNotNull(longClickListener);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.PopupItem.LongPress.Opened",
                                true)
                        .build();

        assertTrue(longClickListener.onLongClick(placeholderView));

        verify(mPopupCoordinator)
                .showContextMenuPopup(any(), eq(placeholderView), eq(new Point(50, 60)), eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testPopupMenuItemLongClickListener_ContextMenuDisabled() {
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        mBookmarkModel.addBookmark(desktopFolderId, 0, "Popup Bookmark", JUnitTestGURLs.URL_1);

        ModelList modelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, desktopFolderId);
        ListItem listItem = modelList.get(0);
        View placeholderView = new View(mActivity);

        View.OnLongClickListener longClickListener =
                listItem.model.get(ListMenuItemProperties.LONG_CLICK_LISTENER);
        assertNotNull(longClickListener);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Bookmarks.BookmarkBar.ContextMenu.PopupItem.LongPress.Opened")
                        .build();

        assertFalse(longClickListener.onLongClick(placeholderView));

        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenInNewTab() {
        BookmarkId id = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.openInNewTab(id);

        verify(mBookmarkOpener)
                .openBookmarksInNewTabs(
                        eq(List.of(id)), eq(false), eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenInNewWindow() {
        BookmarkId id = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.openInNewWindow(id);

        verify(mBookmarkOpener).openBookmarksInNewWindow(eq(List.of(id)), eq(false));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenInIncognitoWindow() {
        BookmarkId id = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.openInIncognitoWindow(id);

        verify(mBookmarkOpener).openBookmarksInNewWindow(eq(List.of(id)), eq(true));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenBookmarksInNewTabs() {
        BookmarkId id1 = new BookmarkId(1, BookmarkType.NORMAL);
        BookmarkId id2 = new BookmarkId(2, BookmarkType.NORMAL);
        List<BookmarkId> ids = List.of(id1, id2);

        mMediator.openBookmarksInNewTabs(ids);

        verify(mBookmarkOpener)
                .openBookmarksInNewTabs(
                        eq(ids), eq(false), eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenBookmarksInNewWindow() {
        BookmarkId id1 = new BookmarkId(1, BookmarkType.NORMAL);
        BookmarkId id2 = new BookmarkId(2, BookmarkType.NORMAL);
        List<BookmarkId> ids = List.of(id1, id2);

        mMediator.openBookmarksInNewWindow(ids);

        verify(mBookmarkOpener).openBookmarksInNewWindow(eq(ids), eq(false));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenBookmarksInIncognitoWindow() {
        BookmarkId id1 = new BookmarkId(1, BookmarkType.NORMAL);
        BookmarkId id2 = new BookmarkId(2, BookmarkType.NORMAL);
        List<BookmarkId> ids = List.of(id1, id2);

        mMediator.openBookmarksInIncognitoWindow(ids);

        verify(mBookmarkOpener).openBookmarksInNewWindow(eq(ids), eq(true));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenBookmarksInNewTabGroup() {
        BookmarkId id1 = new BookmarkId(1, BookmarkType.NORMAL);
        BookmarkId id2 = new BookmarkId(2, BookmarkType.NORMAL);
        List<BookmarkId> ids = List.of(id1, id2);
        String title = "Test Group Title";

        mMediator.openBookmarksInNewTabGroup(ids, title);

        verify(mBookmarkOpener).openBookmarksInNewTabGroup(eq(ids), eq(false), eq(title));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenFolderInNewTabs() {
        BookmarkId folderId = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.openFolderInNewTabs(folderId);

        verify(mBookmarkOpener)
                .openFolderBookmarksInNewTabs(
                        eq(folderId), eq(false), eq(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenFolderInNewWindow() {
        BookmarkId folderId = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.openFolderInNewWindow(folderId);

        verify(mBookmarkOpener).openFolderBookmarksInNewWindow(eq(folderId), eq(false));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenFolderInIncognitoWindow() {
        BookmarkId folderId = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.openFolderInIncognitoWindow(folderId);

        verify(mBookmarkOpener).openFolderBookmarksInNewWindow(eq(folderId), eq(true));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenFolderInNewTabGroup() {
        BookmarkId folderId = new BookmarkId(1, BookmarkType.NORMAL);
        String title = "Test Group Title";

        mMediator.openFolderInNewTabGroup(folderId, title);

        verify(mBookmarkOpener)
                .openFolderBookmarksInNewTabGroup(eq(folderId), eq(false), eq(title));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_EditBookmark() {
        BookmarkId id = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.editBookmark(id);

        verify(mBookmarkManagerOpener).startEditActivity(eq(mActivity), eq(mProfile), eq(id));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_MoveBookmark() {
        BookmarkId id = new BookmarkId(1, BookmarkType.NORMAL);

        mMediator.moveBookmark(id);

        verify(mBookmarkManagerOpener)
                .startFolderPickerActivity(eq(mActivity), eq(mProfile), eq(id));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_DeleteBookmark() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(),
                        0,
                        "Bookmark to Delete",
                        JUnitTestGURLs.URL_1);

        ShadowLooper.idleMainLooper();
        mMediator.deleteBookmark(bookmarkId);

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        assertNotNull(snackbar);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_AddPage() {
        BookmarkId parentId = mBookmarkModel.getDesktopFolderId();
        doReturn("Test Title").when(mTab).getTitle();
        doReturn(JUnitTestGURLs.URL_1).when(mTab).getUrl();

        mMediator.addPage(parentId);

        List<BookmarkId> children = mBookmarkModel.getChildIds(parentId);
        assertEquals(1, children.size());
        BookmarkId newBookmarkId = children.get(0);
        BookmarkItem item = mBookmarkModel.getBookmarkById(newBookmarkId);
        assertNotNull(item);
        assertEquals("Test Title", item.getTitle());
        assertEquals(JUnitTestGURLs.URL_1, item.getUrl());

        verify(mBookmarkManagerOpener)
                .startEditActivity(eq(mActivity), eq(mProfile), eq(newBookmarkId));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_AddFolder() {
        BookmarkId parentId = mBookmarkModel.getDesktopFolderId();

        mMediator.addFolder(parentId);

        verify(mModalDialogManager)
                .showDialog(any(PropertyModel.class), eq(ModalDialogManager.ModalDialogType.APP));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_OpenBookmarksManager() {
        BookmarkId folderId = mBookmarkModel.getDesktopFolderId();

        mMediator.openBookmarksManager(folderId);

        verify(mBookmarkManagerOpener)
                .showBookmarkManager(eq(mActivity), eq(mTab), eq(mProfile), eq(folderId));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testContextMenu_ToggleBookmarksBar() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, true)
                .apply();

        mMediator.toggleBookmarksBar();

        assertFalse(
                ContextUtils.getAppSharedPreferences()
                        .getBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, true));
    }

    @Test
    @SmallTest
    public void testOnAllBookmarksButtonClick() {
        ArgumentCaptor<ClickWithMetaStateCallback> clickCallbackCaptor =
                ArgumentCaptor.forClass(ClickWithMetaStateCallback.class);
        verify(mAllBookmarksButtonModel)
                .set(eq(BookmarkBarButtonProperties.CLICK_CALLBACK), clickCallbackCaptor.capture());

        ClickWithMetaStateCallback clickCallback = clickCallbackCaptor.getValue();
        assertNotNull(clickCallback);

        clickCallback.onClickWithMeta(0, 0);

        verify(mBookmarkManagerOpener)
                .showBookmarkManager(
                        eq(mActivity),
                        eq(mTab),
                        eq(mProfile),
                        eq(mBookmarkModel.getDefaultFolderViewLocation()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT)
    public void testOnAllBookmarksButtonClick_desktopLayoutEnabled() {
        ArgumentCaptor<ClickWithMetaStateCallback> clickCallbackCaptor =
                ArgumentCaptor.forClass(ClickWithMetaStateCallback.class);
        verify(mAllBookmarksButtonModel)
                .set(eq(BookmarkBarButtonProperties.CLICK_CALLBACK), clickCallbackCaptor.capture());

        ClickWithMetaStateCallback clickCallback = clickCallbackCaptor.getValue();
        assertNotNull(clickCallback);

        clickCallback.onClickWithMeta(0, 0);

        verify(mBookmarkManagerOpener)
                .showBookmarkManager(
                        eq(mActivity),
                        eq(mTab),
                        eq(mProfile),
                        eq(mBookmarkModel.getDesktopFolderId()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
    public void testOnAllBookmarksButtonClick_RightClickOrLongPress_NoContextMenu() {
        ArgumentCaptor<ClickWithMetaStateCallback> clickCallbackCaptor =
                ArgumentCaptor.forClass(ClickWithMetaStateCallback.class);
        verify(mAllBookmarksButtonModel)
                .set(eq(BookmarkBarButtonProperties.CLICK_CALLBACK), clickCallbackCaptor.capture());

        ClickWithMetaStateCallback clickCallback = clickCallbackCaptor.getValue();
        assertNotNull(clickCallback);

        View allBookmarksButton = new View(mActivity);
        allBookmarksButton.setId(R.id.bookmark_bar_all_bookmarks_button);
        when(mBookmarkBarView.findViewById(R.id.bookmark_bar_all_bookmarks_button))
                .thenReturn(allBookmarksButton);

        // Simulate right click or long press (BUTTON_SECONDARY).
        clickCallback.onClickWithMeta(0, MotionEvent.BUTTON_SECONDARY);

        // No response should happen for right click on the All bookmarks button.
        verify(mPopupCoordinator, never()).showContextMenuPopup(any(), any(), any(), anyBoolean());
        verify(mBookmarkManagerOpener, never()).showBookmarkManager(any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testOnOverflowButtonClick() {
        ArgumentCaptor<Runnable> callbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mPropertyModel)
                .set(
                        eq(BookmarkBarProperties.OVERFLOW_BUTTON_CLICK_CALLBACK),
                        callbackCaptor.capture());

        Runnable callback = callbackCaptor.getValue();
        assertNotNull(callback);

        FrameLayout overflowButtonView = mock(FrameLayout.class);
        when(mBookmarkBarView.getOverflowButton()).thenReturn(overflowButtonView);

        callback.run();

        verify(mPopupCoordinator)
                .showFolderItemsPopup(eq(overflowButtonView), any(ModelList.class), eq(false));
    }

    @Test
    @SmallTest
    public void testOnProfileChange_ClearsState() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(),
                        0,
                        "Bookmark to Delete",
                        JUnitTestGURLs.URL_1);

        mMediator.deleteBookmark(bookmarkId);

        Profile newProfile = mock(Profile.class);
        when(newProfile.getOriginalProfile()).thenReturn(newProfile);
        when(mUserPrefsJni.get(newProfile)).thenReturn(mPrefService);

        mProfileSupplier.set(newProfile);
        ShadowLooper.idleMainLooper();

        assertNull(mBookmarkModel.getBookmarkById(bookmarkId));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR)
    public void testEmptyFolder_ShowsEmptyItem() {
        BookmarkId emptyFolderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");
        ModelList menuModelList =
                mMediator.buildMenuModelListForFolder(mBookmarkModel, emptyFolderId);
        assertEquals(1, menuModelList.size());
        assertEquals(
                mActivity.getString(R.string.bookmarks_bar_empty_message),
                menuModelList.get(0).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.style.TextAppearance_TextMedium_Disabled,
                menuModelList.get(0).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertFalse(menuModelList.get(0).model.get(ListMenuItemProperties.ENABLED));
    }
}
