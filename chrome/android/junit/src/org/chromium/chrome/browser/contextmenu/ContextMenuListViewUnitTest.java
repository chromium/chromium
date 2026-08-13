// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.components.embedder_support.contextmenu.ContextMenuSwitches;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link ContextMenuListView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuListViewUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
    }

    private static class FixedWidthAdapter extends BaseAdapter {
        private final Activity mActivity;
        private final int mItemWidth;

        FixedWidthAdapter(Activity activity, int itemWidth) {
            mActivity = activity;
            mItemWidth = itemWidth;
        }

        @Override
        public int getCount() {
            return 1;
        }

        @Override
        public Object getItem(int position) {
            return "item";
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            TextView tv = new TextView(mActivity);
            tv.setText("item");
            tv.setMinimumWidth(mItemWidth);
            return tv;
        }
    }

    @Test
    @Config(qualifiers = "w400dp-h800dp-mdpi")
    public void testDialogMode_ScalesWithContent_WithinBounds() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        int contentItemWidth = 250;
        listView.setAdapter(new FixedWidthAdapter(mActivity, contentItemWidth));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        Assert.assertEquals(
                "Dialog width should match content width when within bounds",
                contentItemWidth,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w400dp-h800dp-mdpi")
    public void testDialogMode_ScalesWithContent_ClampedToMin() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setAdapter(new FixedWidthAdapter(mActivity, 100));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int minWidth = mActivity.getResources().getDimensionPixelSize(R.dimen.menu_width_min);
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Dialog width should clamp up to menu_width_min",
                minWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w400dp-h800dp-mdpi")
    public void testDialogMode_ScalesWithContent_ClampedToScreenSpace() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setAdapter(new FixedWidthAdapter(mActivity, 450));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int margin = mActivity.getResources().getDimensionPixelSize(R.dimen.menu_horizontal_margin);
        int expectedMenuWidth = 400 - 2 * margin;
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Dialog width should be constrained by window space",
                expectedMenuWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h400dp-mdpi")
    public void testDialogMode_CappedAtMaxMenuWidth() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setAdapter(new FixedWidthAdapter(mActivity, 900));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int maxMenuWidth = mActivity.getResources().getDimensionPixelSize(R.dimen.menu_width_max);
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Dialog width should be capped at menu_width_max even on wide displays",
                maxMenuWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h800dp-mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testPopupMode_ScalesWithContent_ClampedToMin() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setAdapter(new FixedWidthAdapter(mActivity, 100));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int minWidth = mActivity.getResources().getDimensionPixelSize(R.dimen.menu_width_min);
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Popup should clamp up to menu_width_min",
                minWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h800dp-mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testPopupMode_ScalesWithContent_WithinBounds() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        int contentItemWidth = 350;
        listView.setAdapter(new FixedWidthAdapter(mActivity, contentItemWidth));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        Assert.assertEquals(
                "Popup width should match content width",
                contentItemWidth,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h800dp-mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testPopupMode_ScalesWithContent_ClampedToMax() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setAdapter(new FixedWidthAdapter(mActivity, 900));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int maxMenuWidth = mActivity.getResources().getDimensionPixelSize(R.dimen.menu_width_max);
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Popup should clamp down to menu_width_max",
                maxMenuWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h800dp-mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testFlyoutMode_ClampedToFlyoutMaxWidth() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setIsFlyout(true);
        listView.setAdapter(new FixedWidthAdapter(mActivity, 450));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int flyoutMaxWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.flyout_menu_max_width);
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Flyout popup should clamp down to flyout_menu_max_width",
                flyoutMaxWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h800dp-mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testFlyoutMode_ScalesWithContent_ClampedToMin() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        listView.setIsFlyout(true);
        listView.setAdapter(new FixedWidthAdapter(mActivity, 100));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        int minWidth = mActivity.getResources().getDimensionPixelSize(R.dimen.menu_width_min);
        int parentLateralPadding = root.getPaddingLeft() + root.getPaddingRight();

        Assert.assertEquals(
                "Flyout popup should clamp up to menu_width_min",
                minWidth - parentLateralPadding,
                listView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "w1000dp-h800dp-mdpi")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void testFlyoutMode_ScalesWithContent_WithinBounds() {
        View root = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ContextMenuListView listView = root.findViewById(R.id.context_menu_list_view);

        int contentItemWidth = 250;
        listView.setIsFlyout(true);
        listView.setAdapter(new FixedWidthAdapter(mActivity, contentItemWidth));
        listView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        Assert.assertEquals(
                "Flyout width should match content width when within bounds",
                contentItemWidth,
                listView.getMeasuredWidth());
    }
}
