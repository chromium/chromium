// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.PAGE_KEY_LISTENER;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Robolectric tests for {@link TabGridDialogViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGridDialogViewBinderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private TabGridDialogViewBinder.ViewHolder mViewHolder;
    @Mock private TabGridDialogToolbarView mToolbarView;
    @Mock private TabListRecyclerView mContentView;
    @Mock private TabGridDialogView mDialogView;
    @Mock Callback<TabKeyEventData> mPageKeyEventDataCallback;

    @Before
    public void setUp() {
        mViewHolder =
                new TabGridDialogViewBinder.ViewHolder(mToolbarView, mContentView, mDialogView);
    }

    @Test
    public void testPageKeyListenerCallback() {
        PropertyModel propertyModel =
                spy(
                        new PropertyModel.Builder(TabGridDialogProperties.ALL_KEYS)
                                .with(PAGE_KEY_LISTENER, mPageKeyEventDataCallback)
                                .build());

        TabGridDialogViewBinder.bind(propertyModel, mViewHolder, PAGE_KEY_LISTENER);

        verify(mContentView, times(1)).setPageKeyListenerCallback(mPageKeyEventDataCallback);
    }
}
