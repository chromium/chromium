// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.drawable.Drawable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.TextView;

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

    @Mock private SearchBoxContainerView mSearchBoxLayout;
    @Mock private View mSearchBoxContainer;
    @Mock private TextView mSearchBoxTextView;

    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mPropertyModel = new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mSearchBoxLayout, new SearchBoxViewBinder());
        when(mSearchBoxLayout.findViewById(R.id.search_box_container))
                .thenReturn(mSearchBoxContainer);
        when(mSearchBoxLayout.findViewById(R.id.search_box_text)).thenReturn(mSearchBoxTextView);
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
    public void testApplyWhiteBackground() {
        mPropertyModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND, true);
        verify(mSearchBoxLayout).applyWhiteBackground(eq(true));

        mPropertyModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND, false);
        verify(mSearchBoxLayout).applyWhiteBackground(eq(false));
    }

    @Test
    public void testSetDseIconResource() {
        int resId = 123;
        mPropertyModel.set(SearchBoxProperties.DSE_ICON_RESOURCE_ID, resId);
        verify(mSearchBoxLayout).setDseIconResource(eq(resId));
    }

    @Test
    public void testSetDseIconDrawable() {
        Drawable drawable = mock(Drawable.class);
        mPropertyModel.set(SearchBoxProperties.DSE_ICON_DRAWABLE, drawable);
        verify(mSearchBoxLayout).setDseIconDrawable(eq(drawable));
    }

    @Test
    public void testSetTextWatcher() {
        TextWatcher textWatcher = mock(TextWatcher.class);
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, textWatcher);
        verify(mSearchBoxTextView).addTextChangedListener(eq(textWatcher));

        clearInvocations(mSearchBoxTextView);
        when(mSearchBoxTextView.getTag(R.id.ntp_search_box_text_watcher_tag))
                .thenReturn(textWatcher);
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, null);
        verify(mSearchBoxTextView).removeTextChangedListener(eq(textWatcher));
        verify(mSearchBoxTextView, never()).addTextChangedListener(any(TextWatcher.class));
    }
}
