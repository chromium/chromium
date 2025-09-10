// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for the {@link BookmarkBarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarMediatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModel mPropertyModel;
    @Mock private ModelList mItemsModel;
    @Mock private PropertyModel mAllBookmarksButtonModel;
    @Mock private ObservableSupplierImpl<Boolean> mItemsOverflowSupplier;
    @Mock private Profile mProfile;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private RecyclerView mItemsRecyclerView;
    @Mock private BookmarkBar mBookmarkBarView;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private AnchoredPopupWindow mAnchoredPopupWindow;
    @Mock private BasicListMenu mMockListMenu;
    @Mock private BookmarkBarItemsLayoutManager mLayoutManager;

    private Activity mActivity;
    private BookmarkBarMediator mMediator;
    private FakeBookmarkModel mBookmarkModel;
    private ObservableSupplierImpl<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkModel = FakeBookmarkModel.createModel();
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        mProfileSupplier = new ObservableSupplierImpl<>(mProfile);
        Supplier<Pair<Integer, Integer>> controlsHeightSupplier = () -> new Pair<>(0, 0);
        when(mLayoutManager.getItemsOverflowSupplier()).thenReturn(mItemsOverflowSupplier);

        mMediator =
                new BookmarkBarMediator(
                        mActivity,
                        mAllBookmarksButtonModel,
                        controlsHeightSupplier,
                        mItemsModel,
                        mLayoutManager,
                        mPropertyModel,
                        mProfileSupplier,
                        /* currentTab= */ null,
                        mBookmarkOpener,
                        new ObservableSupplierImpl<>(mBookmarkManagerOpener),
                        mItemsRecyclerView,
                        mBookmarkBarView);
    }

    @After
    public void tearDown() throws Exception {
        mMediator.destroy();
        assertNull(BookmarkBarMediator.sFolderIconBitmap);
    }

    // Tests the behavior of sFolderIconBitmap.
    @Test
    @SmallTest
    public void testStaticFolderIconBitmap() throws Exception {
        // Create a new folder inside the bookmarks bar.
        BookmarkId desktopFolderId = mBookmarkModel.getDesktopFolderId();
        BookmarkId rootFolderId = mBookmarkModel.addFolder(desktopFolderId, 0, "Root Folder");

        // Add a child folder to the root folder to ensure the caching logic is triggered.
        mBookmarkModel.addFolder(rootFolderId, 0, "Child Folder");

        assertNull("Cache should be empty initially.", BookmarkBarMediator.sFolderIconBitmap);

        // Trigger #createListItemForBookmarkFolder, which populates the sFolderIconBitmap cache.
        mMediator.buildMenuModelListForFolder(mBookmarkModel, rootFolderId);

        assertNotNull("Cache should be populated.", BookmarkBarMediator.sFolderIconBitmap);

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

        // Verify the second item (F2), which should be a submenu.
        ListItem f2ListItem = modelList.get(1);
        assertEquals(ListItemType.MENU_ITEM_WITH_SUBMENU, f2ListItem.type);
        assertEquals("F2", f2ListItem.model.get(ListMenuItemProperties.TITLE));

        // Verify the structure of the submenu.
        List<ListItem> submenuItems =
                f2ListItem.model.get(ListMenuSubmenuItemProperties.SUBMENU_ITEMS);
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
    public void testSetupEmptyView() {
        // Create a fake content view structure that mirrors the real one.
        ViewGroup contentParent = new LinearLayout(mActivity);
        ListView menuList = new ListView(mActivity);
        menuList.setId(R.id.menu_list);
        contentParent.addView(menuList);

        // The list layout has only one child with no empty view.
        assertEquals(1, contentParent.getChildCount());
        assertNull(menuList.getEmptyView());

        mMediator.setupEmptyView(contentParent);

        // Verify that a second child was added.
        assertEquals(2, contentParent.getChildCount());

        // Check that that second child is an empty view.
        View emptyView = menuList.getEmptyView();
        assertNotNull("The empty view should be set on the ListView.", emptyView);
        assertEquals(
                "The empty view's parent should be the contentParent.",
                contentParent,
                emptyView.getParent());
        assertEquals(
                "The empty view should have the correct message.",
                mActivity.getString(R.string.bookmarks_bar_empty_message),
                ((TextView) emptyView).getText());

        // Verify that calling it again doesn't add another view.
        mMediator.setupEmptyView(contentParent);
        assertEquals("Should not add a second empty view", 2, contentParent.getChildCount());
    }

    @Test
    @SmallTest
    public void testConfigurePopupWindowSize_smokeTest() {
        mMediator.setAnchoredPopupWindowForTesting(mAnchoredPopupWindow);
        when(mMockListMenu.getMenuDimensions()).thenReturn(new int[] {300, 100});

        mMediator.configurePopupWindowSize(mMockListMenu, 5);

        // Verify that the setDesiredContentSize method was called at least once with
        // any integer values.
        verify(mAnchoredPopupWindow, Mockito.atLeastOnce())
                .setDesiredContentSize(Mockito.anyInt(), Mockito.anyInt());
    }
}
