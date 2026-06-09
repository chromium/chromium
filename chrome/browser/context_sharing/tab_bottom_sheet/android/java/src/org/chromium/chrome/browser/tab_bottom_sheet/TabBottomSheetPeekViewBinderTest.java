// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.view.View.OnClickListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetPeekViewBinderTest {
    private static final String TEST_STRING = "TEST_STRING";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabBottomSheetPeekView mView;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mChangeProcessor;

    private OnClickListener mActionButtonClickListener;
    private OnClickListener mCloseClickListener;
    private OnClickListener mPeekViewClickListener;

    private boolean mClicked;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS).build();
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, TabBottomSheetPeekViewBinder::bind);

        doAnswer(
                        invocation -> {
                            mActionButtonClickListener = invocation.getArgument(0);
                            return null;
                        })
                .when(mView)
                .setActionButtonClickListener(any());

        doAnswer(
                        invocation -> {
                            mCloseClickListener = invocation.getArgument(0);
                            return null;
                        })
                .when(mView)
                .setCloseClickListener(any());

        doAnswer(
                        invocation -> {
                            mPeekViewClickListener = invocation.getArgument(0);
                            return null;
                        })
                .when(mView)
                .setPeekViewClickListener(any());
    }

    @Test
    public void testTitleText() {
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, TEST_STRING);
        verify(mView).setTitle(TEST_STRING);
    }

    @Test
    public void testTitleTextAppearance() {
        int styleRes = 456;
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID, styleRes);
        verify(mView).setTitleTextAppearance(styleRes);
    }

    @Test
    public void testDescriptionText() {
        int descRes = 123;
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID, descRes);
        verify(mView).setDescriptionText(descRes);
    }

    @Test
    public void testDescriptionVisibility() {
        int visibility = 8;
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, visibility);
        verify(mView).setDescriptionVisibility(visibility);
    }

    @Test
    public void testActionButtonText() {
        int textRes = 123;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID, textRes);
        verify(mView).setActionButtonText(textRes);
    }

    @Test
    public void testActionButtonVisibility() {
        int visibility = 0;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, visibility);
        verify(mView).setActionButtonVisibility(visibility);
    }

    @Test
    public void testActionButtonIcon() {
        int iconRes = 789;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_ID, iconRes);
        verify(mView).setActionButtonIcon(iconRes);
    }

    @Test
    public void testActionButtonBackgroundTint() {
        int colorRes = 111;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID, colorRes);
        verify(mView).setActionButtonBackgroundTint(colorRes);
    }

    @Test
    public void testActionButtonIconTint() {
        int colorRes = 222;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT_ID, colorRes);
        verify(mView).setActionButtonIconTint(colorRes);
    }

    @Test
    public void testActionButtonHorizontalPadding() {
        int paddingRes = 333;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING_ID, paddingRes);
        verify(mView).setActionButtonHorizontalPadding(paddingRes);
    }

    @Test
    public void testActionButtonContentDescription() {
        int descRes = 123;
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION_ID, descRes);
        verify(mView).setActionButtonContentDescription(descRes);
    }

    @Test
    public void testOnActionButtonClicked() {
        mClicked = false;
        mModel =
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(
                                TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED,
                                () -> mClicked = true)
                        .build();
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, TabBottomSheetPeekViewBinder::bind);

        assertNotNull(mActionButtonClickListener);
        mActionButtonClickListener.onClick(null);
        assertTrue(mClicked);
    }

    @Test
    public void testOnCloseClicked() {
        mClicked = false;
        mModel =
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED, () -> mClicked = true)
                        .build();
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, TabBottomSheetPeekViewBinder::bind);

        assertNotNull(mCloseClickListener);
        mCloseClickListener.onClick(null);
        assertTrue(mClicked);
    }

    @Test
    public void testOnPeekViewClicked() {
        mClicked = false;
        mModel =
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(
                                TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED,
                                () -> mClicked = true)
                        .build();
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, TabBottomSheetPeekViewBinder::bind);

        assertNotNull(mPeekViewClickListener);
        mPeekViewClickListener.onClick(null);
        assertTrue(mClicked);
    }
}
