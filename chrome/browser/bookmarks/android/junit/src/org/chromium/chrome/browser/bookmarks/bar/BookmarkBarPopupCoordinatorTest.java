// Copyright 2026 The Chromium Authors
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

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for the {@link BookmarkBarPopupCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarPopupCoordinatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AnchoredPopupWindow mAnchoredPopupWindow;
    @Mock private BasicListMenu mMockListMenu;
    @Mock private View mBookmarkBarView;

    private Activity mActivity;
    private BookmarkBarPopupCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mCoordinator =
                new BookmarkBarPopupCoordinator(
                        mActivity,
                        mBookmarkBarView,
                        ObservableSuppliers.createMonotonic(),
                        () -> new Pair<>(0, 0), // controlsHeightSupplier
                        /* currentTabSupplier= */ () -> null);
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

        mCoordinator.setupEmptyView(contentParent);

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
        mCoordinator.setupEmptyView(contentParent);
        assertEquals("Should not add a second empty view", 2, contentParent.getChildCount());
    }

    @Test
    @SmallTest
    public void testConfigurePopupWindowSize_smokeTest() {
        mCoordinator.setAnchoredPopupWindowForTesting(mAnchoredPopupWindow);
        when(mMockListMenu.getMenuDimensions()).thenReturn(new int[] {300, 100});

        mCoordinator.configurePopupWindowSize(mMockListMenu);

        // Verify that the setDesiredContentSize method was called at least once with
        // any integer values.
        verify(mAnchoredPopupWindow, Mockito.atLeastOnce())
                .setDesiredContentSize(Mockito.anyInt(), Mockito.anyInt());
    }
}
