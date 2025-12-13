// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.widget.TextView;

import com.airbnb.lottie.LottieAnimationView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link SearchBoxViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchBoxViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View.OnClickListener mOnClickListener;

    @Mock private SearchBoxContainerView mSearchBoxLayout;
    @Mock private View mSearchBoxContainer;
    @Mock private LottieAnimationView mComposeplateButtonView;
    @Mock private TextView mSearchBoxTextView;

    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mPropertyModel = new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mSearchBoxLayout, new SearchBoxViewBinder());
        when(mSearchBoxLayout.findViewById(R.id.search_box_container))
                .thenReturn(mSearchBoxContainer);
        when(mSearchBoxLayout.findViewById(R.id.composeplate_button))
                .thenReturn(mComposeplateButtonView);
        when(mSearchBoxLayout.findViewById(R.id.search_box_text)).thenReturn(mSearchBoxTextView);
    }

    @Test
    public void testSetComposeplateButtonClickListener() {
        mPropertyModel.set(
                SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK, mOnClickListener);
        verify(mComposeplateButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetComposeplateButtonVisibility() {
        mPropertyModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY, true);
        verify(mSearchBoxLayout).setComposeplateButtonVisibility(eq(true));

        mPropertyModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY, false);
        verify(mSearchBoxLayout).setComposeplateButtonVisibility(eq(false));
    }

    @Test
    public void testSetComposeplateButtonAnimation() {
        int iconRawResId = 10;
        mPropertyModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID, iconRawResId);
        verify(mComposeplateButtonView).setAnimation(eq(iconRawResId));
    }

    @Test
    public void testSetSearchBoxEndPadding() {
        int padding = 20;
        when(mSearchBoxContainer.getPaddingLeft()).thenReturn(10);
        when(mSearchBoxContainer.getPaddingTop()).thenReturn(10);
        when(mSearchBoxContainer.getPaddingBottom()).thenReturn(10);
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_END_PADDING, padding);
        verify(mSearchBoxContainer).setPadding(10, 10, padding, 10);
    }

    @Test
    public void testSetSearchBoxStartPadding() {
        int padding = 20;
        when(mSearchBoxContainer.getPaddingTop()).thenReturn(10);
        when(mSearchBoxContainer.getPaddingEnd()).thenReturn(10);
        when(mSearchBoxContainer.getPaddingBottom()).thenReturn(10);
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_START_PADDING, padding);
        verify(mSearchBoxContainer).setPadding(padding, 10, 10, 10);
    }

    @Test
    public void testSetSearchBoxTextStyle() {
        int resId = 123;
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
        verify(mSearchBoxTextView).setTextAppearance(eq(resId));
    }

    @Test
    public void testEnableSearchBoxEditText() {
        mPropertyModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, true);
        verify(mSearchBoxTextView).setEnabled(eq(true));

        mPropertyModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, false);
        verify(mSearchBoxTextView).setEnabled(eq(false));
    }

    @Test
    public void testSetSearchBoxHintText() {
        String hintText = "new hint";
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, hintText);
        verify(mSearchBoxTextView).setHint(eq(hintText));
    }

    @Test
    public void testApplyWhiteBackgroundWithShadow() {
        mPropertyModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, true);
        verify(mSearchBoxLayout).applyWhiteBackgroundWithShadow(eq(true));

        mPropertyModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, false);
        verify(mSearchBoxLayout).applyWhiteBackgroundWithShadow(eq(false));
    }
}
