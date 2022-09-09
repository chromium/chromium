// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link TabGridDialogMenuItemBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGridDialogMenuItemBinderUnitTest {
    @Mock
    private TextView mMockTextView;

    @Mock
    private TextView mMockRootView;

    private static final String FAKE_ITEM_TITLE_TEXT = "FAKE TITLE";
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(TabGridDialogMenuItemProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mMockRootView, TabGridDialogMenuItemBinder::bind);

        when(mMockRootView.findViewById(R.id.menu_item_text)).thenReturn(mMockTextView);
    }

    @Test
    @SmallTest
    public void updateTitleText() {
        mModel.set(TabGridDialogMenuItemProperties.TITLE, FAKE_ITEM_TITLE_TEXT);
        verify(mMockTextView, times(1)).setText(FAKE_ITEM_TITLE_TEXT);

        mModel.set(TabGridDialogMenuItemProperties.TITLE, null);
        verify(mMockTextView, times(1)).setText(null);
    }
}
