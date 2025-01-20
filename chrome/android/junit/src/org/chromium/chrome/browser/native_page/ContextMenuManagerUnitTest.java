// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPopupWindow;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.ui.base.TestActivity;

/** Unit test for {@link ContextMenuManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowPopupWindow.class)
public class ContextMenuManagerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ContextMenuManager mManager;
    private View mAnchorView;

    @Mock NativePageNavigationDelegate mNavigationDelegate;
    @Mock TouchEnabledDelegate mTouchEnabledDelegate;
    @Mock ContextMenuManager.Delegate mDelegate;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mAnchorView = spy(new View(mActivity, null));
        mManager = new ContextMenuManager(mNavigationDelegate, mTouchEnabledDelegate, () -> {}, "");
    }

    @Test
    public void emptyListContextMenu() {
        assertFalse(
                "showContextMenu failed since list is empty.",
                mManager.showListContextMenu(mAnchorView, mDelegate));
    }

    @Test
    public void showListContextMenu() {
        doReturn(true).when(mDelegate).isItemSupported(anyInt());
        doReturn(false).when(mNavigationDelegate).isOpenInNewTabInGroupEnabled();
        doReturn(false).when(mNavigationDelegate).isOpenInNewWindowEnabled();
        doReturn(false).when(mNavigationDelegate).isOpenInIncognitoEnabled();
        doReturn(null).when(mDelegate).getUrl();
        doReturn(true).when(mAnchorView).isAttachedToWindow();

        assertTrue(
                "showContextMenu failed since list is empty.",
                mManager.showListContextMenu(mAnchorView, mDelegate));
        assertNotNull("List context menu is null.", mManager.getListMenuForTesting());
        verify(mDelegate).onContextMenuCreated();
    }
}
