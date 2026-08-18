// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ChromePopupWindow;
import org.chromium.ui.widget.UiWidgetFactory;

/** Unit tests for {@link BookmarkBarPopup}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarPopupTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AnchoredPopupWindow mAnchoredPopupWindow;
    @Mock private BasicListMenu mMockListMenu;

    @Mock private View mAnchorView;
    @Mock private ChromePopupWindow mMockPopupWindow;
    @Captor private ArgumentCaptor<Drawable> mDrawableCaptor;

    private Activity mActivity;
    private BookmarkBarPopup mPopup;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        when(mMockPopupWindow.getBackground()).thenReturn(new ColorDrawable(Color.TRANSPARENT));

        mPopup = new BookmarkBarPopup(mActivity, () -> new Pair<>(0, 0)); // controlsHeightSupplier
    }

    @Test
    @SmallTest
    public void testSetupEmptyView() {
        ViewGroup contentParent = new LinearLayout(mActivity);
        ListView menuList = new ListView(mActivity);
        menuList.setId(R.id.menu_list);
        contentParent.addView(menuList);

        assertEquals(1, contentParent.getChildCount());
        assertNull(menuList.getEmptyView());

        mPopup.setupEmptyView(contentParent);

        assertEquals(2, contentParent.getChildCount());

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

        mPopup.setupEmptyView(contentParent);
        assertEquals("Should not add a second empty view", 2, contentParent.getChildCount());
    }

    @Test
    @SmallTest
    public void testConfigurePopupWindowSize_measuredWidthLessThanMax() {
        mPopup.setPopupWindowForTesting(mAnchoredPopupWindow);
        setupMockListMenuWithContent();

        android.content.res.Resources resources = mActivity.getResources();
        int maxWidthPx = resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_max_width);
        int widthPixels = resources.getDisplayMetrics().widthPixels;
        int expectedFinalWidth = Math.min(maxWidthPx, widthPixels);

        int measuredWidth = expectedFinalWidth - 100;
        int measuredHeight = 200;
        when(mMockListMenu.getMenuDimensions())
                .thenReturn(new int[] {measuredWidth, measuredHeight});
        when(mMockListMenu.getMaxItemWidth()).thenReturn(measuredWidth);

        mPopup.configurePopupWindowSize(mAnchoredPopupWindow, mMockListMenu);

        verify(mAnchoredPopupWindow).setDesiredContentSize(measuredWidth, measuredHeight);
    }

    @Test
    @SmallTest
    public void testConfigurePopupWindowSize_measuredWidthGreaterThanMax() {
        mPopup.setPopupWindowForTesting(mAnchoredPopupWindow);
        setupMockListMenuWithContent();

        android.content.res.Resources resources = mActivity.getResources();
        int maxWidthPx = resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_max_width);
        int widthPixels = resources.getDisplayMetrics().widthPixels;
        int expectedFinalWidth = Math.min(maxWidthPx, widthPixels);

        int measuredWidth = expectedFinalWidth + 100;
        int measuredHeight = 200;
        when(mMockListMenu.getMenuDimensions())
                .thenReturn(new int[] {measuredWidth, measuredHeight});
        when(mMockListMenu.getMaxItemWidth()).thenReturn(measuredWidth);

        mPopup.configurePopupWindowSize(mAnchoredPopupWindow, mMockListMenu);

        verify(mAnchoredPopupWindow).setDesiredContentSize(expectedFinalWidth, measuredHeight);
    }

    @Test
    @SmallTest
    public void testConfigurePopupWindowSize_measuredLessThanMin() {
        mPopup.setPopupWindowForTesting(mAnchoredPopupWindow);
        setupMockListMenuWithContent();

        android.content.res.Resources resources = mActivity.getResources();
        int minInteractTargetSizePx =
                resources.getDimensionPixelSize(R.dimen.min_touch_target_size);
        int marginPx = (int) Math.ceil(resources.getDisplayMetrics().density);
        int expectedMinSizePx = minInteractTargetSizePx + 2 * marginPx;

        int measuredWidth = expectedMinSizePx - 10;
        int measuredHeight = expectedMinSizePx - 10;
        when(mMockListMenu.getMenuDimensions())
                .thenReturn(new int[] {measuredWidth, measuredHeight});
        when(mMockListMenu.getMaxItemWidth()).thenReturn(measuredWidth);

        mPopup.configurePopupWindowSize(mAnchoredPopupWindow, mMockListMenu);

        verify(mAnchoredPopupWindow).setDesiredContentSize(expectedMinSizePx, expectedMinSizePx);
    }

    private void setupMockListMenuWithContent() {
        ModelListAdapter mockAdapter = Mockito.mock(ModelListAdapter.class);
        when(mockAdapter.getCount()).thenReturn(1);
        when(mMockListMenu.getContentAdapter()).thenReturn(mockAdapter);

        ViewGroup contentParent = new LinearLayout(mActivity);
        ListView menuList = new ListView(mActivity);
        menuList.setId(R.id.menu_list);
        contentParent.addView(menuList);
        when(mMockListMenu.getContentView()).thenReturn(contentParent);
    }

    @Test
    @SmallTest
    public void testShow_usesTransparentBackground() {
        View rootView = new View(mActivity);
        when(mAnchorView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getViewTreeObserver()).thenReturn(rootView.getViewTreeObserver());

        UiWidgetFactory originalFactory = UiWidgetFactory.getInstance();
        UiWidgetFactory.setInstance(
                new UiWidgetFactory() {
                    @Override
                    public ChromePopupWindow createPopupWindow(Context context) {
                        return mMockPopupWindow;
                    }
                });

        try {
            mPopup.show(
                    mAnchorView,
                    new ModelList(),
                    /* isIncognito= */ false,
                    /* dismissAllCallback= */ () -> {},
                    /* onDismissListener= */ () -> {},
                    /* touchListener= */ null,
                    /* touchInterceptor= */ null);

            verify(mMockPopupWindow).setBackgroundDrawable(mDrawableCaptor.capture());

            assertTrue(mDrawableCaptor.getValue() instanceof ColorDrawable);
            assertEquals(
                    Color.TRANSPARENT, ((ColorDrawable) mDrawableCaptor.getValue()).getColor());
        } finally {
            UiWidgetFactory.setInstance(originalFactory);
        }
    }

    @Test
    @SmallTest
    public void testShowAtOffset_usesTransparentBackground() {
        View rootView = new View(mActivity);
        when(mAnchorView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getViewTreeObserver()).thenReturn(rootView.getViewTreeObserver());

        UiWidgetFactory originalFactory = UiWidgetFactory.getInstance();
        UiWidgetFactory.setInstance(
                new UiWidgetFactory() {
                    @Override
                    public ChromePopupWindow createPopupWindow(Context context) {
                        return mMockPopupWindow;
                    }
                });

        try {
            mPopup.showAtOffset(
                    mAnchorView,
                    new android.graphics.Point(10, 20),
                    new ModelList(),
                    /* isIncognito= */ false,
                    /* dismissAllCallback= */ () -> {},
                    /* onDismissListener= */ () -> {},
                    /* touchListener= */ null,
                    /* touchInterceptor= */ null);

            verify(mMockPopupWindow).setBackgroundDrawable(mDrawableCaptor.capture());

            assertTrue(mDrawableCaptor.getValue() instanceof ColorDrawable);
            assertEquals(
                    Color.TRANSPARENT, ((ColorDrawable) mDrawableCaptor.getValue()).getColor());
        } finally {
            UiWidgetFactory.setInstance(originalFactory);
        }
    }
}
