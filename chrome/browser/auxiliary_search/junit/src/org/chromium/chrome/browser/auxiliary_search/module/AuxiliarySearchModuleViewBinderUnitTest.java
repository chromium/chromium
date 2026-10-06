// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link AuxiliarySearchModuleViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchModuleViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private OnClickListener mOnClickListener;

    private Context mContext;
    private AuxiliarySearchModuleView mView;
    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (AuxiliarySearchModuleView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.auxiliary_search_module_layout, null);
        mPropertyModel =
                new PropertyModel.Builder(AuxiliarySearchModuleProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mView, AuxiliarySearchModuleViewBinder::bind);
    }

    private TextView getTextView(int id) {
        return mView.findViewById(id);
    }

    @Test
    public void testClickListeners() {
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER,
                mOnClickListener);
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER,
                mOnClickListener);

        TextView firstButton = getTextView(R.id.auxiliary_search_first_button);
        firstButton.performClick();
        verify(mOnClickListener).onClick(firstButton);

        TextView secondButton = getTextView(R.id.auxiliary_search_second_button);
        secondButton.performClick();
        verify(mOnClickListener).onClick(secondButton);
    }

    @Test
    public void testResIds() {
        int titleResId = R.string.auxiliary_search_browsing_data_module_name;
        int contentResId = R.string.auxiliary_search_module_content_default_off;
        int firstButtonResId = R.string.auxiliary_search_module_button_no_thanks;
        int secondButtonResId = R.string.auxiliary_search_module_button_turn_on;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_TITLE_TEXT_RES_ID, titleResId);
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID, contentResId);
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID, firstButtonResId);
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID,
                secondButtonResId);

        assertEquals(
                mContext.getString(titleResId),
                getTextView(R.id.auxiliary_search_module_title).getText().toString());
        assertEquals(
                mContext.getString(contentResId),
                getTextView(R.id.auxiliary_search_module_content).getText().toString());
        assertEquals(
                mContext.getString(firstButtonResId),
                getTextView(R.id.auxiliary_search_first_button).getText().toString());
        assertEquals(
                mContext.getString(secondButtonResId),
                getTextView(R.id.auxiliary_search_second_button).getText().toString());
    }
}
