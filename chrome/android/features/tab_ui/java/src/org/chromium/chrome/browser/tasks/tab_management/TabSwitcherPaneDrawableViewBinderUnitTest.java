// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherPaneDrawableProperties.TAB_COUNT;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabSwitcherPaneDrawableViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneDrawableViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSwitcherDrawable mDrawable;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(TabSwitcherPaneDrawableProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mModel, mDrawable, TabSwitcherPaneDrawableViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testBindAllProperties() {
        int tabCount = 5;
        mModel.set(TAB_COUNT, tabCount);
        verify(mDrawable).updateForTabCount(eq(tabCount), eq(false));
    }
}
