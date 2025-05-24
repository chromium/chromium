// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerTestingDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.content_public.browser.test.util.TouchCommon;

/** Tests for the bookmark manager which include partner bookmarks. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PartnerBookmarkTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ShoppingService mShoppingService;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;

    private BookmarkModel mBookmarkModel;
    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private RecyclerView mItemsContainer;
    private BookmarkDelegate mBookmarkDelegate;
    private BookmarkManagerTestingDelegate mBookmarkManagerTestingDelegate;
    private DragReorderableRecyclerViewAdapter mAdapter;

    @Before
    public void setUp() {
        // Setup the shopping service.
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        mActivityTestRule.startOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel =
                            BookmarkModel.getForProfile(
                                    mActivityTestRule.getProfile(/* incognito= */ false));
                    mBookmarkModel.loadFakePartnerBookmarkShimForTesting();
                    mBookmarkModel.finishLoadingBookmarkModel(CallbackUtils.emptyRunnable());
                });

        // Exclude the BookmarkPromoHeader for a consistent testing setup.
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);

        mBookmarkManagerCoordinator =
                mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        mBookmarkDelegate = mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
        mBookmarkManagerTestingDelegate = mBookmarkManagerCoordinator.getTestingDelegate();
        mItemsContainer = mBookmarkManagerCoordinator.getRecyclerViewForTesting();
        mAdapter = (DragReorderableRecyclerViewAdapter) mItemsContainer.getAdapter();
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneForPartnerBookmarks() {
        // Open partner bookmarks folder.
        BookmarkId partnerFolder = runOnUiThreadBlocking(() -> mBookmarkModel.getPartnerFolderId());
        mBookmarkTestRule.openFolder(partnerFolder);

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertEquals(
                "Wrong number of items in partner bookmark folder.",
                2,
                mBookmarkManagerTestingDelegate.getBookmarkCount());

        // Verify that bookmark 1 is editable (so more button can be triggered) but not movable.
        BookmarkId partnerBookmarkId1 =
                mBookmarkManagerTestingDelegate.getBookmarkIdByPositionForTesting(0);
        runOnUiThreadBlocking(
                () -> {
                    BookmarkItem partnerBookmarkItem1 =
                            mBookmarkModel.getBookmarkById(partnerBookmarkId1);
                    partnerBookmarkItem1.forceEditableForTesting();
                    assertEquals(
                            "Incorrect bookmark type for item 1",
                            BookmarkType.PARTNER,
                            partnerBookmarkId1.getType());
                    assertFalse(
                            "Partner item 1 should not be movable",
                            BookmarkUtils.isMovable(mBookmarkModel, partnerBookmarkItem1));
                    assertTrue(
                            "Partner item 1 should be editable", partnerBookmarkItem1.isEditable());
                });

        // Verify that bookmark 1 is editable (so more button can be triggered) but not movable.
        View partnerBookmarkView1 = mBookmarkManagerTestingDelegate.getBookmarkRowByPosition(0);
        View more1 = partnerBookmarkView1.findViewById(R.id.more);
        assertEquals(View.GONE, more1.getVisibility());

        // Verify that bookmark 2 is not movable.
        BookmarkId partnerBookmarkId2 =
                mBookmarkManagerTestingDelegate.getBookmarkIdByPositionForTesting(1);
        runOnUiThreadBlocking(
                () -> {
                    BookmarkItem partnerBookmarkItem2 =
                            mBookmarkModel.getBookmarkById(partnerBookmarkId2);
                    partnerBookmarkItem2.forceEditableForTesting();
                    assertEquals(
                            "Incorrect bookmark type for item 2",
                            BookmarkType.PARTNER,
                            partnerBookmarkId2.getType());
                    assertFalse(
                            "Partner item 2 should not be movable",
                            BookmarkUtils.isMovable(mBookmarkModel, partnerBookmarkItem2));
                    assertTrue(
                            "Partner item 2 should be editable", partnerBookmarkItem2.isEditable());
                });

        // Verify that bookmark 2 does not have move up/down items.
        View partnerBookmarkView2 = mBookmarkManagerTestingDelegate.getBookmarkRowByPosition(1);
        View more2 = partnerBookmarkView2.findViewById(R.id.more);
        assertEquals(View.GONE, more2.getVisibility());
    }

    @Test
    @MediumTest
    public void testCannotSelectPartner() throws Exception {
        mBookmarkTestRule.openFolder(mBookmarkTestRule.getMobileFolder());
        View partner = mBookmarkManagerTestingDelegate.getBookmarkViewHolderByPosition(0).itemView;
        TouchCommon.longPressView(partner);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        assertFalse(
                "Expected that we would not be in selection mode "
                        + "after long pressing on partner bookmark.",
                mBookmarkDelegate.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testPartnerFolderDraggability() throws Exception {
        mBookmarkTestRule.openFolder(mBookmarkTestRule.getMobileFolder());
        ViewHolder partner = mBookmarkManagerTestingDelegate.getBookmarkViewHolderByPosition(0);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        assertFalse(
                "Partner bookmarks folder should not be passively draggable",
                isViewHolderPassivelyDraggable(partner));
        assertFalse(
                "Partner bookmarks folder should not be actively draggable",
                isViewHoldersActivelyDraggable(partner));
    }

    private boolean isViewHolderPassivelyDraggable(ViewHolder viewHolder) {
        return runOnUiThreadBlocking(() -> mAdapter.isPassivelyDraggable(viewHolder));
    }

    private boolean isViewHoldersActivelyDraggable(ViewHolder viewHolder) {
        return runOnUiThreadBlocking(() -> mAdapter.isActivelyDraggable(viewHolder));
    }
}
