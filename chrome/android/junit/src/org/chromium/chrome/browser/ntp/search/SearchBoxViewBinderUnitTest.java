// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;
import androidx.test.core.app.ApplicationProvider;

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

    @Mock private TextWatcher mTextWatcher;
    @Mock private OnClickListener mOnClickListener;

    private Context mContext;
    private SearchBoxContainerView mSearchBoxLayout;
    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mSearchBoxLayout =
                (SearchBoxContainerView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.fake_search_box_layout, null);
        mPropertyModel = new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mSearchBoxLayout, new SearchBoxViewBinder());
    }

    @Test
    public void testSetSearchBoxEndPadding() {
        @Px int padding = 20;
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_END_PADDING, padding);
        assertEquals(padding, mSearchBoxLayout.getPaddingRight());
    }

    @Test
    public void testSetSearchBoxTextStyle() {
        @StyleRes int resId = R.style.TextAppearance_TextLarge_Secondary;
        @ColorInt int previousTextColor = mSearchBoxLayout.mHintTextView.getCurrentTextColor();
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
        assertNotEquals(previousTextColor, mSearchBoxLayout.mHintTextView.getCurrentTextColor());
    }

    @Test
    public void testEnableSearchBoxEditText() {
        mPropertyModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, true);
        assertTrue(mSearchBoxLayout.mHintTextView.isEnabled());

        mPropertyModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, false);
        assertFalse(mSearchBoxLayout.mHintTextView.isEnabled());
    }

    @Test
    public void testSetSearchBoxHintText() {
        String hintText = "new hint";
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, hintText);
        assertEquals(hintText, mSearchBoxLayout.mHintTextView.getHint().toString());
    }

    @Test
    public void testApplyWhiteBackground() {
        mPropertyModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND, true);
        Drawable background = mSearchBoxLayout.getBackground();
        assertTrue(background instanceof GradientDrawable);
        assertEquals(Color.WHITE, ((GradientDrawable) background).getColor().getDefaultColor());
    }

    @Test
    public void testSetDseIconDrawable() {
        Drawable drawable = mContext.getDrawable(R.drawable.ic_search_24dp);
        mPropertyModel.set(SearchBoxProperties.DSE_ICON_DRAWABLE, drawable);
        assertEquals(drawable, mSearchBoxLayout.mDseIconView.getDrawable());
    }

    @Test
    public void testSetTextWatcher() {
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, mTextWatcher);

        mSearchBoxLayout.mHintTextView.setText("test");
        verify(mTextWatcher)
                .onTextChanged(
                        argThat(s -> TextUtils.equals("test", s)), anyInt(), anyInt(), anyInt());

        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, null);
        clearInvocations(mTextWatcher);
        mSearchBoxLayout.mHintTextView.setText("test2");
        verify(mTextWatcher, never()).onTextChanged(any(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testSetPlusButtonVisibility() {
        mPropertyModel.set(SearchBoxProperties.PLUS_BUTTON_VISIBILITY, true);
        assertEquals(View.VISIBLE, mSearchBoxLayout.mPlusButton.getVisibility());
        assertEquals(View.GONE, mSearchBoxLayout.mDseIconView.getVisibility());

        mPropertyModel.set(SearchBoxProperties.PLUS_BUTTON_VISIBILITY, false);
        assertEquals(View.GONE, mSearchBoxLayout.mPlusButton.getVisibility());
        assertEquals(View.VISIBLE, mSearchBoxLayout.mDseIconView.getVisibility());
    }

    @Test
    public void testSetPlusButtonClickListener() {
        mPropertyModel.set(SearchBoxProperties.PLUS_BUTTON_CLICK_CALLBACK, mOnClickListener);
        mSearchBoxLayout.mPlusButton.performClick();
        verify(mOnClickListener).onClick(mSearchBoxLayout.mPlusButton);
    }
}
