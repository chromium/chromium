// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.TextView;

import com.google.android.material.button.MaterialButton;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetPeekViewBinderTest {
    private static final String TEST_STRING = "TEST_STRING";

    private Activity mActivity;
    private TabBottomSheetPeekView mView;
    private PropertyModel mModel;
    private MaterialButton mActionButton;

    private boolean mClicked;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_MaterialComponents);
        mView =
                (TabBottomSheetPeekView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_bottom_sheet_peek_layout, null);
        mActionButton = mView.findViewById(R.id.peek_action_button);
        bindModel(new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS).build());
    }

    private void bindModel(PropertyModel model) {
        mModel = model;
        PropertyModelChangeProcessor.create(mModel, mView, TabBottomSheetPeekViewBinder::bind);
    }

    @Test
    public void testPeekIcon() {
        int iconRes = android.R.drawable.ic_delete;
        mModel.set(TabBottomSheetPeekProperties.PEEK_ICON_ID, iconRes);
        ImageView peekIcon = mView.findViewById(R.id.peek_icon);
        assertEquals(iconRes, Shadows.shadowOf(peekIcon.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testTitleText() {
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, TEST_STRING);
        TextView title = mView.findViewById(R.id.peek_title);
        assertEquals(TEST_STRING, title.getText().toString());
    }

    @Test
    public void testTitleTextAppearance() {
        int styleRes = android.R.style.TextAppearance_Large;
        TextView expected = new TextView(mActivity);
        expected.setTextAppearance(styleRes);

        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID, styleRes);
        TextView title = mView.findViewById(R.id.peek_title);
        assertEquals(expected.getTextSize(), title.getTextSize(), 0f);
    }

    @Test
    public void testDescriptionText() {
        int descRes = android.R.string.ok;
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID, descRes);
        assertEquals(mActivity.getString(descRes), mView.getStepDescriptionForTesting());
    }

    @Test
    public void testDescriptionVisibility() {
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.GONE);
        assertEquals(View.GONE, mView.findViewById(R.id.peek_description).getVisibility());
    }

    @Test
    public void testActionButtonText() {
        int textRes = android.R.string.ok;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID, textRes);
        assertEquals(mActivity.getString(textRes), mActionButton.getText().toString());
    }

    @Test
    public void testActionButtonVisibility() {
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.GONE);
        assertEquals(View.GONE, mActionButton.getVisibility());

        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.VISIBLE);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());
    }

    @Test
    public void testActionButtonIcon() {
        int iconRes = android.R.drawable.ic_delete;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_ID, iconRes);
        assertEquals(iconRes, Shadows.shadowOf(mActionButton.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testActionButtonBackgroundTint() {
        int colorRes = android.R.color.holo_red_dark;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID, colorRes);
        assertEquals(
                mActivity.getColor(colorRes),
                mActionButton.getBackgroundTintList().getDefaultColor());
    }

    @Test
    public void testActionButtonIconTint() {
        int colorRes = android.R.color.holo_blue_dark;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT_ID, colorRes);
        assertEquals(mActivity.getColor(colorRes), mActionButton.getIconTint().getDefaultColor());
    }

    @Test
    public void testActionButtonHorizontalPadding() {
        int paddingRes = android.R.dimen.app_icon_size;
        int expectedPadding = mActivity.getResources().getDimensionPixelSize(paddingRes);
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING_ID, paddingRes);
        assertEquals(expectedPadding, mActionButton.getPaddingStart());
        assertEquals(expectedPadding, mActionButton.getPaddingEnd());
    }

    @Test
    public void testActionButtonContentDescription() {
        int descRes = android.R.string.ok;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION_ID, descRes);
        assertEquals(mActivity.getString(descRes), mActionButton.getContentDescription());
    }

    @Test
    public void testOnActionButtonClicked() {
        mClicked = false;
        bindModel(
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(
                                TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED,
                                () -> mClicked = true)
                        .build());

        mActionButton.performClick();
        assertTrue(mClicked);
    }

    @Test
    public void testOnCloseClicked() {
        mClicked = false;
        bindModel(
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED, () -> mClicked = true)
                        .build());

        mView.findViewById(R.id.peek_close_button).performClick();
        assertTrue(mClicked);
    }

    @Test
    public void testOnPeekViewClicked() {
        mClicked = false;
        bindModel(
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(
                                TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED,
                                () -> mClicked = true)
                        .build());

        mView.performClick();
        assertTrue(mClicked);
    }

    @Test
    public void testAccessibilityProperties() {
        bindModel(
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(
                                TabBottomSheetPeekProperties.CONTENT_DESCRIPTION_A11Y,
                                context -> "Ask Gemini, Gemini in Chrome")
                        .build());

        assertEquals("Ask Gemini, Gemini in Chrome", mView.getContentDescription().toString());

        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mView.onInitializeAccessibilityNodeInfo(info);
        assertEquals(android.widget.Button.class.getName(), info.getClassName().toString());
    }
}
