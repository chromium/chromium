// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;

import java.util.ArrayList;

@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesRecyclerViewUnitTest {
    private static final int WIDTH = 500;
    private static final int HEIGHT = 2000;

    private Activity mActivity;
    private HomeModulesRecyclerView mRecyclerView;
    private int mModuleInternalPaddingPx;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRecyclerView =
                (HomeModulesRecyclerView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.home_modules_recycler_view_layout, null);
        mActivity.setContentView(mRecyclerView);

        mModuleInternalPaddingPx =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.module_internal_padding);
    }

    /** Populates the RecyclerView with the given item views, and lays it out. */
    private void setItemViews(View... views) {
        // The RecyclerView is GONE by default in the layout.
        mRecyclerView.setVisibility(View.VISIBLE);
        for (View view : views) {
            // Give items a non-zero height so that the RecyclerView lays them all out.
            if (view.getLayoutParams() == null) {
                view.setLayoutParams(new RecyclerView.LayoutParams(MATCH_PARENT, 100));
            }
        }
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mActivity));
        mRecyclerView.setAdapter(
                new RecyclerView.Adapter<RecyclerView.ViewHolder>() {
                    @Override
                    public int getItemCount() {
                        return views.length;
                    }

                    @Override
                    public int getItemViewType(int position) {
                        return position;
                    }

                    @Override
                    public RecyclerView.ViewHolder onCreateViewHolder(
                            ViewGroup parent, int viewType) {
                        return new RecyclerView.ViewHolder(views[viewType]) {};
                    }

                    @Override
                    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}
                });
        layout(mRecyclerView);
        assertEquals(views.length, mRecyclerView.getChildCount());
    }

    /** Synchronously measures and lays out the given view, clearing any pending layout request. */
    private static void layout(View view) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(HEIGHT, View.MeasureSpec.AT_MOST));
        view.layout(0, 0, view.getMeasuredWidth(), view.getMeasuredHeight());
        assertFalse(view.isLayoutRequested());
    }

    /** Sets the height of an item view (which must already be laid out by the RecyclerView). */
    private static void setHeight(View view, int height) {
        view.setBottom(view.getTop() + height);
        assertEquals(height, view.getHeight());
    }

    @Test
    public void testOnDraw_OneItermPerScreen() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        int measuredWidth = 500;
        mRecyclerView.initialize(/* isTablet= */ true, startMarginPx, itemPerScreen);

        View view = new View(mActivity);
        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(100, 100);
        view.setLayoutParams(marginLayoutParams);
        layout(view);
        startMarginPx = 5;
        mRecyclerView.setStartMarginPxForTesting(startMarginPx);

        // Verifies when there is one item per screen, the width is set to MATCH_PARENT.
        mRecyclerView.onDrawImplTablet(view, 3, measuredWidth);
        assertEquals(MATCH_PARENT, marginLayoutParams.width);
        assertEquals(startMarginPx, marginLayoutParams.getMarginStart());
        assertEquals(startMarginPx, marginLayoutParams.getMarginEnd());
        assertTrue(view.isLayoutRequested());

        // Verifies that setLayoutParams() is called again to update the margins.
        layout(view);
        mRecyclerView.onDrawImplTablet(view, 3, measuredWidth);
        assertEquals(MATCH_PARENT, marginLayoutParams.width);
        assertTrue(view.isLayoutRequested());
    }

    @Test
    public void testOnDraw_MultipleItemsPerScreen() {
        int itemPerScreen = 2;
        int startMarginPx = 0;
        int measuredWidth = 500;
        mRecyclerView.initialize(/* isTablet= */ true, startMarginPx, itemPerScreen);

        View view = new View(mActivity);
        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(100, 100);
        view.setLayoutParams(marginLayoutParams);
        layout(view);
        startMarginPx = 10;
        mRecyclerView.setStartMarginPxForTesting(startMarginPx);
        int expectedWidth =
                (measuredWidth - mModuleInternalPaddingPx * (itemPerScreen - 1)) / itemPerScreen;

        // Verifies the width becomes the half of the parent's width.
        mRecyclerView.onDrawImplTablet(view, 3, measuredWidth);
        assertEquals(expectedWidth, marginLayoutParams.width);
        assertEquals(startMarginPx, marginLayoutParams.getMarginStart());
        assertEquals(startMarginPx, marginLayoutParams.getMarginEnd());
        assertTrue(view.isLayoutRequested());

        // Verifies that setLayoutParams() isn't called again whether there isn't any change to the
        // width of the view.
        layout(view);
        mRecyclerView.onDrawImplTablet(view, 3, measuredWidth);
        assertEquals(expectedWidth, marginLayoutParams.width);
        assertFalse(view.isLayoutRequested());
    }

    @Test
    public void testAddFocusables() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);

        ArrayList<View> views = new ArrayList<>();

        mRecyclerView.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        mRecyclerView.setFocusable(true);
        mRecyclerView.addFocusables(views, View.FOCUS_FORWARD, View.FOCUSABLES_ALL);

        assertEquals(1, views.size());
        assertEquals(mRecyclerView, views.get(0));
    }

    @Test
    public void testFocusSearch() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);

        // A module containing a single focusable view.
        FrameLayout moduleView = new FrameLayout(mActivity);
        moduleView.setLayoutParams(new RecyclerView.LayoutParams(MATCH_PARENT, 100));
        View focused = new View(mActivity);
        focused.setFocusable(true);
        moduleView.addView(focused, new FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        setItemViews(moduleView);
        int descendantFocusability = mRecyclerView.getDescendantFocusability();
        boolean isFocusable = mRecyclerView.isFocusable();

        // Verify FOCUS_FORWARD exhausts the current card and triggers the escape sequence. Since
        // there is nothing else to focus on the page, the focused view is returned.
        View result = mRecyclerView.focusSearch(focused, View.FOCUS_FORWARD);
        assertEquals(focused, result);

        // Verify descendant focusability and focusability were restored after the escape.
        assertEquals(descendantFocusability, mRecyclerView.getDescendantFocusability());
        assertEquals(isFocusable, mRecyclerView.isFocusable());
    }

    @Test
    public void testFocusSearch_escapesToNextViewOutside() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);

        // Place a focusable view after the RecyclerView.
        LinearLayout root = new LinearLayout(mActivity);
        root.setOrientation(LinearLayout.VERTICAL);
        ((ViewGroup) mRecyclerView.getParent()).removeView(mRecyclerView);
        root.addView(mRecyclerView, new LinearLayout.LayoutParams(MATCH_PARENT, 100));
        View nextView = new View(mActivity);
        nextView.setFocusable(true);
        root.addView(nextView, new LinearLayout.LayoutParams(MATCH_PARENT, 100));
        mActivity.setContentView(root);

        FrameLayout moduleView = new FrameLayout(mActivity);
        moduleView.setLayoutParams(new RecyclerView.LayoutParams(MATCH_PARENT, 100));
        View focused = new View(mActivity);
        focused.setFocusable(true);
        moduleView.addView(focused, new FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        setItemViews(moduleView);
        RobolectricUtil.runAllBackgroundAndUi();

        // Focus escapes the RecyclerView (rather than going back into its descendants) and moves
        // to the next view on the page.
        assertEquals(nextView, mRecyclerView.focusSearch(focused, View.FOCUS_FORWARD));
    }

    @Test
    public void testGetMaxHeight() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);
        int miniModuleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.home_module_height);

        View view = new View(mActivity);
        setItemViews(view);

        setHeight(view, miniModuleHeight - 10);
        assertEquals(miniModuleHeight, mRecyclerView.getMaxHeight());

        int height = miniModuleHeight * 2;
        setHeight(view, height);
        assertEquals(height, mRecyclerView.getMaxHeight());
    }

    @Test
    public void testUpdateHeight_OneChild() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);
        int miniModuleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.home_module_height);
        int childCount = 1;
        View view = new View(mActivity);
        setItemViews(view);

        int height = miniModuleHeight * 2;
        setHeight(view, height);
        assertEquals(height, mRecyclerView.getMaxHeight());
        mRecyclerView.updateHeight(childCount);
        // Verifies requestLayout() isn't called if the child view is higher than the minimal
        // height.
        assertFalse(mRecyclerView.isLayoutRequested());
        assertEquals(0, view.getMinimumHeight());

        height = miniModuleHeight - 10;
        setHeight(view, height);
        assertEquals(miniModuleHeight, mRecyclerView.getMaxHeight());
        mRecyclerView.updateHeight(childCount);
        // Verifies requestLayout() is called to change the height of the child view to be the
        // minimal height.
        assertTrue(mRecyclerView.isLayoutRequested());
        assertEquals(miniModuleHeight, view.getMinimumHeight());
    }

    @Test
    public void testUpdateHeight_TwoChildren() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);
        int miniModuleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.home_module_height);

        View view = new View(mActivity);
        View view1 = new View(mActivity);
        setItemViews(view, view1);

        int height = miniModuleHeight * 2;
        int height1 = miniModuleHeight + 1;
        setHeight(view, height);
        setHeight(view1, height1);
        int childCount = 2;
        assertEquals(height, mRecyclerView.getMaxHeight());

        mRecyclerView.updateHeight(childCount);
        // Verifies that requestLayout() is called to adjust the minimal height of the child view
        // with lower height.
        assertEquals(0, view.getMinimumHeight());
        assertEquals(height, view1.getMinimumHeight());
        assertTrue(mRecyclerView.isLayoutRequested());
    }
}
