// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import static org.mockito.ArgumentMatchers.eq;
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

/** Unit tests for {@link AuxiliarySearchModuleViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchModuleViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AuxiliarySearchModuleView mView;
    @Mock private OnClickListener mOnClickListener;

    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mPropertyModel =
                new PropertyModel.Builder(AuxiliarySearchModuleProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mView, AuxiliarySearchModuleViewBinder::bind);
    }

    @Test
    public void testSetFirstButtonClickListener() {
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER,
                mOnClickListener);
        verify(mView).setFirstButtonOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetSecondButtonClickListener() {
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER,
                mOnClickListener);
        verify(mView).setSecondButtonOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetTitleTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_TITLE_TEXT_RES_ID, resId);
        verify(mView).setTitleTextResId(eq(resId));
    }

    @Test
    public void testSetContentTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID, resId);
        verify(mView).setContentTextResId(eq(resId));
    }

    @Test
    public void testSetFirstButtonTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID, resId);
        verify(mView).setFirstButtonTextResId(eq(resId));
    }

    @Test
    public void testSetSecondButtonTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID, resId);
        verify(mView).setSecondButtonTextResId(eq(resId));
    }
}
