// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.ImageView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpChromeColorsLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpChromeColorsLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpChromeColorsAdapter mAdapter;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private TextWatcher mTextWatcher;
    @Mock private OnCheckedChangeListener mOnCheckedChangeListener;

    private View mLayoutView;
    private NtpChromeColorGridRecyclerView mRecyclerView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mLayoutView =
                LayoutInflater.from(activity)
                        .inflate(
                                R.layout.ntp_customization_chrome_colors_bottom_sheet_layout,
                                /* root= */ null);
        mRecyclerView = mLayoutView.findViewById(R.id.chrome_colors_recycler_view);

        mModel = new PropertyModel(NtpChromeColorsProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mLayoutView, NtpChromeColorsLayoutViewBinder::bind);
    }

    @Test
    public void testSetBackClickListener() {
        View backButton = mLayoutView.findViewById(R.id.back_button);
        mModel.set(NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER, mOnClickListener);
        assertEquals(mOnClickListener, shadowOf(backButton).getOnClickListener());
    }

    @Test
    public void testSetSaveClickListener() {
        View saveButton = mLayoutView.findViewById(R.id.save_color_button);
        mModel.set(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER, mOnClickListener);
        assertEquals(mOnClickListener, shadowOf(saveButton).getOnClickListener());
    }

    @Test
    public void testSetBackgroundColorInputWatcher() {
        EditText input = mLayoutView.findViewById(R.id.background_color_input);
        mModel.set(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER, mTextWatcher);
        input.setText("FF0000");
        verify(mTextWatcher).afterTextChanged(any());
    }

    @Test
    public void testSetPrimaryColorInputWatcher() {
        EditText input = mLayoutView.findViewById(R.id.primary_color_input);
        mModel.set(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER, mTextWatcher);
        input.setText("FF0000");
        verify(mTextWatcher).afterTextChanged(any());
    }

    @Test
    public void testSetBackgroundColorCircle() {
        ImageView circle = mLayoutView.findViewById(R.id.background_color_circle);
        assertNotEquals(View.VISIBLE, circle.getVisibility());

        mModel.set(NtpChromeColorsProperties.BACKGROUND_COLOR_CIRCLE_VIEW_COLOR, Color.BLUE);

        assertEquals(Color.BLUE, getCircleColor(circle));
        assertEquals(View.VISIBLE, circle.getVisibility());
    }

    @Test
    public void testSetPrimaryColorCircle() {
        ImageView circle = mLayoutView.findViewById(R.id.primary_color_circle);
        assertNotEquals(View.VISIBLE, circle.getVisibility());

        mModel.set(NtpChromeColorsProperties.PRIMARY_COLOR_CIRCLE_VIEW_COLOR, Color.RED);

        assertEquals(Color.RED, getCircleColor(circle));
        assertEquals(View.VISIBLE, circle.getVisibility());
    }

    @Test
    public void testSetCustomColorPickerContainerVisibility() {
        View container = mLayoutView.findViewById(R.id.custom_color_picker_container);
        mModel.set(
                NtpChromeColorsProperties.CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY, View.VISIBLE);
        assertEquals(View.VISIBLE, container.getVisibility());

        mModel.set(NtpChromeColorsProperties.CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY, View.GONE);
        assertEquals(View.GONE, container.getVisibility());
    }

    @Test
    public void testSetRecyclerViewLayoutManager() {
        LayoutManager layoutManager =
                new GridLayoutManager(mLayoutView.getContext(), /* spanCount= */ 1);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_LAYOUT_MANAGER, layoutManager);
        assertEquals(layoutManager, mRecyclerView.getLayoutManager());
    }

    @Test
    public void testSetRecyclerViewAdapter() {
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER, mAdapter);
        assertEquals(mAdapter, mRecyclerView.getAdapter());
    }

    @Test
    public void testSetRecyclerViewItemWidth() {
        GridLayoutManager layoutManager = setUpGrid(/* spacing= */ 0, /* maxItemCount= */ 100);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH, 10);

        measureRecyclerView(/* width= */ 95);

        // 95 / (10 + 0) = 9 columns.
        assertEquals(9, layoutManager.getSpanCount());
        assertEquals(90, mRecyclerView.getMeasuredWidth());
    }

    @Test
    public void testSetRecyclerViewSpacing() {
        GridLayoutManager layoutManager = setUpGrid(/* spacing= */ 20, /* maxItemCount= */ 100);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH, 10);

        measureRecyclerView(/* width= */ 95);

        // 95 / (10 + 20) = 3 columns.
        assertEquals(3, layoutManager.getSpanCount());
        assertEquals(90, mRecyclerView.getMeasuredWidth());
    }

    @Test
    public void testSetRecyclerViewMaxItemCount() {
        GridLayoutManager layoutManager = setUpGrid(/* spacing= */ 0, /* maxItemCount= */ 4);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH, 10);

        measureRecyclerView(/* width= */ 95);

        // 9 columns would fit, but the max item count caps it to 4.
        assertEquals(4, layoutManager.getSpanCount());
        assertEquals(40, mRecyclerView.getMeasuredWidth());
    }

    @Test
    public void testSetDailyRefreshSwitchChecked() {
        MaterialSwitchWithText dailyRefreshSwitch =
                mLayoutView.findViewById(R.id.chrome_colors_switch_button);
        mModel.set(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, true);
        assertTrue(dailyRefreshSwitch.isChecked());

        mModel.set(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, false);
        assertFalse(dailyRefreshSwitch.isChecked());
    }

    @Test
    public void testSetDailyRefreshSwitchOnCheckedChangeListener() {
        MaterialSwitchWithText dailyRefreshSwitch =
                mLayoutView.findViewById(R.id.chrome_colors_switch_button);
        mModel.set(
                NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                mOnCheckedChangeListener);
        dailyRefreshSwitch.setChecked(true);
        verify(mOnCheckedChangeListener).onCheckedChanged(any(), eq(true));
    }

    @Test
    public void testSetHighlightedItemIndex() {
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER, mAdapter);

        int index = 0;
        mModel.set(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX, index);
        verify(mAdapter).setSelectedPosition(eq(index), eq(false));

        // Verifies the setSelectedPosition() will be called again for the same index value.
        mModel.set(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX, index);
        verify(mAdapter, times(2)).setSelectedPosition(eq(index), eq(false));
    }

    private GridLayoutManager setUpGrid(int spacing, int maxItemCount) {
        GridLayoutManager layoutManager =
                new GridLayoutManager(mLayoutView.getContext(), /* spanCount= */ 1);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_LAYOUT_MANAGER, layoutManager);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_SPACING, spacing);
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_MAX_ITEM_COUNT, maxItemCount);
        return layoutManager;
    }

    private void measureRecyclerView(int width) {
        mRecyclerView.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
    }

    private static int getCircleColor(ImageView circle) {
        ColorStateList color = ((GradientDrawable) circle.getBackground()).getColor();
        return color.getDefaultColor();
    }
}
