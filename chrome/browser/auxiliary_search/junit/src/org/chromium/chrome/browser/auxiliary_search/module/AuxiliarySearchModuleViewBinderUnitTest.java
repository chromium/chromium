// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.view.View.OnClickListener;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link AuxiliarySearchModuleViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    @SmallTest
    public void testSetFirstButtonClickListener() {
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER,
                mOnClickListener);
        verify(mView).setFirstButtonOnClickListener(eq(mOnClickListener));
    }

    @Test
    @SmallTest
    public void testSetSecondButtonClickListener() {
        mPropertyModel.set(
                AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER,
                mOnClickListener);
        verify(mView).setSecondButtonOnClickListener(eq(mOnClickListener));
    }

    @Test
    @SmallTest
    public void testSetContentTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID, resId);
        verify(mView).setContentTextResId(eq(resId));
    }

    @Test
    @SmallTest
    public void testSetFirstButtonTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID, resId);
        verify(mView).setFirstButtonTextResId(eq(resId));
    }

    @Test
    @SmallTest
    public void testSetSecondButtonTextResId() {
        int resId = 10;
        mPropertyModel.set(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID, resId);
        verify(mView).setSecondButtonTextResId(eq(resId));
    }
}
