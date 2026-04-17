// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabSwitcherButtonView;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabSwitcherActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherActionButtonBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSwitcherButtonView mView;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(TabSwitcherActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, TabSwitcherActionButtonBinder::bind);
    }

    @Test
    @SmallTest
    public void testTabCount() {
        mModel.set(TabSwitcherActionProperties.TAB_COUNT, 5);
        verify(mView).setTabCount(5, false);
    }

    @Test
    @SmallTest
    public void testIsIncognito() {
        mModel.set(TabSwitcherActionProperties.IS_INCOGNITO, true);
        verify(mView).setTabCount(0, true);
    }

    @Test
    @SmallTest
    public void testHasNotificationDot() {
        mModel.set(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT, true);
        verify(mView).setNotificationDotVisible(true);

        mModel.set(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT, false);
        verify(mView).setNotificationDotVisible(false);
    }

    @Test
    @SmallTest
    public void testShowTabSwitcherTrigger() {
        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        verify(mView).endRippleAnimation();
    }

    @Test
    @SmallTest
    public void testShowTabSwitcherTrigger_MultipleTimes() {
        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
        verify(mView, times(3)).endRippleAnimation();
    }
}
