// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static junit.framework.Assert.assertEquals;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpChromeColorsLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpChromeColorsLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mLayoutView;
    @Mock private ConstraintLayout mConstraintLayout;
    @Mock private View mBackButton;
    @Mock private ImageView mLearnMoreButton;
    @Mock private ImageView mSaveButton;
    @Mock private EditText mBackgroundColorInput;
    @Mock private EditText mPrimaryColorInput;
    @Mock private ImageView mBackgroundColorCircleView;
    @Mock private ImageView mPrimaryColorCircleView;
    @Mock private View mCustomColorPickerContainer;
    @Mock private NtpChromeColorGridRecyclerView mRecyclerView;
    @Mock private FrameLayout mRecyclerViewContainer;
    @Mock private LayoutManager mLayoutManager;
    @Mock private Adapter<RecyclerView.ViewHolder> mAdapter;
    @Mock private GradientDrawable mGradientDrawable;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private TextWatcher mTextWatcher;
    @Mock private ConstraintSet mConstraintSet;
    @Mock private MaterialSwitchWithText mDailyRefreshSwitch;
    @Mock private OnCheckedChangeListener mOnCheckedChangeListener;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel(NtpChromeColorsProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mLayoutView, NtpChromeColorsLayoutViewBinder::bind);

        when(mLayoutView.findViewById(R.id.back_button)).thenReturn(mBackButton);
        when(mLayoutView.findViewById(R.id.learn_more_button)).thenReturn(mLearnMoreButton);
        when(mLayoutView.findViewById(R.id.save_button)).thenReturn(mSaveButton);
        when(mLayoutView.findViewById(R.id.background_color_input))
                .thenReturn(mBackgroundColorInput);
        when(mLayoutView.findViewById(R.id.primary_color_input)).thenReturn(mPrimaryColorInput);
        when(mLayoutView.findViewById(R.id.background_color_circle))
                .thenReturn(mBackgroundColorCircleView);
        when(mLayoutView.findViewById(R.id.primary_color_circle))
                .thenReturn(mPrimaryColorCircleView);
        when(mLayoutView.findViewById(R.id.custom_color_picker_container))
                .thenReturn(mCustomColorPickerContainer);
        when(mLayoutView.findViewById(R.id.chrome_colors_recycler_view)).thenReturn(mRecyclerView);
        when(mLayoutView.findViewById(R.id.chrome_colors_recycler_view_container))
                .thenReturn(mRecyclerViewContainer);
        when(mLayoutView.findViewById(R.id.chrome_colors_switch_button))
                .thenReturn(mDailyRefreshSwitch);

        when(mBackgroundColorCircleView.getBackground()).thenReturn(mGradientDrawable);
        when(mPrimaryColorCircleView.getBackground()).thenReturn(mGradientDrawable);
        when(mGradientDrawable.mutate()).thenReturn(mGradientDrawable);
    }

    @Test
    public void testSetBackClickListener() {
        mModel.set(NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER, mOnClickListener);
        verify(mBackButton).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetLearnMoreClickListener() {
        mModel.set(NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER, mOnClickListener);
        verify(mLearnMoreButton).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetSaveClickListener() {
        mModel.set(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER, mOnClickListener);
        verify(mSaveButton).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetBackgroundColorInputWatcher() {
        mModel.set(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER, mTextWatcher);
        verify(mBackgroundColorInput).addTextChangedListener(eq(mTextWatcher));
    }

    @Test
    public void testSetPrimaryColorInputWatcher() {
        mModel.set(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER, mTextWatcher);
        verify(mPrimaryColorInput).addTextChangedListener(eq(mTextWatcher));
    }

    @Test
    public void testSetBackgroundColorCircle() {
        mModel.set(NtpChromeColorsProperties.BACKGROUND_COLOR_CIRCLE_VIEW_COLOR, Color.BLUE);

        verify(mGradientDrawable).setColor(Color.BLUE);
        verify(mBackgroundColorCircleView).setVisibility(View.VISIBLE);
    }

    @Test
    public void testSetPrimaryColorCircle() {
        mModel.set(NtpChromeColorsProperties.PRIMARY_COLOR_CIRCLE_VIEW_COLOR, Color.RED);

        verify(mGradientDrawable).setColor(Color.RED);
        verify(mPrimaryColorCircleView).setVisibility(View.VISIBLE);
    }

    @Test
    public void testSetCustomColorPickerContainerVisibility() {
        mModel.set(NtpChromeColorsProperties.CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY, View.GONE);
        verify(mCustomColorPickerContainer).setVisibility(View.GONE);

        mModel.set(
                NtpChromeColorsProperties.CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY, View.VISIBLE);
        verify(mCustomColorPickerContainer).setVisibility(View.VISIBLE);
    }

    @Test
    public void testSetRecyclerViewLayoutManager() {
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_LAYOUT_MANAGER, mLayoutManager);
        verify(mRecyclerView).setLayoutManager(eq(mLayoutManager));
    }

    @Test
    public void testSetRecyclerViewAdapter() {
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER, mAdapter);
        verify(mRecyclerView).setAdapter(eq(mAdapter));
    }

    @Test
    public void testSetRecyclerViewItemWidth() {
        int itemWidth = 10;
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH, itemWidth);
        verify(mRecyclerView).setItemWidth(eq(itemWidth));
    }

    @Test
    public void testSetRecyclerViewSpacing() {
        int spacing = 20;
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_SPACING, spacing);
        verify(mRecyclerView).setSpacing(eq(spacing));
    }

    @Test
    public void testSetRecyclerViewMaxWidth() {
        int maxWidthPx = 100;
        mModel.set(NtpChromeColorsProperties.RECYCLER_VIEW_MAX_WIDTH_PX, maxWidthPx);
        assertEquals(maxWidthPx, mModel.get(NtpChromeColorsProperties.RECYCLER_VIEW_MAX_WIDTH_PX));
    }

    @Test
    public void testSetDailyRefreshSwitchChecked() {
        mModel.set(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, true);
        verify(mDailyRefreshSwitch).setChecked(eq(true));

        mModel.set(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, false);
        verify(mDailyRefreshSwitch).setChecked(eq(false));
    }

    @Test
    public void testSetDailyRefreshSwitchOnCheckedChangeListener() {
        mModel.set(
                NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                mOnCheckedChangeListener);
        verify(mDailyRefreshSwitch).setOnCheckedChangeListener(eq(mOnCheckedChangeListener));
    }

    @Test
    public void testSetConstraintSet() {
        int id = 10;
        int maxWidthPx = 100;
        when(mConstraintLayout.findViewById(R.id.chrome_colors_recycler_view_container))
                .thenReturn(mRecyclerViewContainer);
        when(mRecyclerViewContainer.getId()).thenReturn(id);

        NtpChromeColorsLayoutViewBinder.setConstraintSet(
                mConstraintSet, mConstraintLayout, mRecyclerViewContainer, maxWidthPx);

        verify(mConstraintSet).clone(eq(mConstraintLayout));
        verify(mConstraintSet).constrainedWidth(eq(id), eq(true));
        verify(mConstraintSet).constrainMaxWidth(eq(id), eq(maxWidthPx));
        verify(mConstraintSet).applyTo(eq(mConstraintLayout));
    }
}
