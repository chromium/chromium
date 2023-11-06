// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.UserData;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link BottomSheetFocusHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class BottomSheetFocusHelperRoboelectricTest {
    private BottomSheetFocusHelper mBottomSheetFocusHelper;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WebContents mWebContents;
    @Mock private WebContentsAccessibility mWebContentsAccessibility;
    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private UserData mUserData;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mBottomSheetFocusHelper =
                new BottomSheetFocusHelper(mBottomSheetController, mWindowAndroid);
        mBottomSheetFocusHelper.setWebContentsAccessibility(mWebContentsAccessibility);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
    }

    @Test
    public void testRegisterForOneTimeUseAddsTheObserver() {
        mBottomSheetFocusHelper.registerForOneTimeUse();
        // Verify that BottomSheetFocusHelper added itself as an observer of the BottomSheet.
        verify(mBottomSheetController, times(1)).addObserver(mBottomSheetFocusHelper);
    }

    @Test
    public void testOnSheetClosedRemovesTheObserver() {
        mBottomSheetFocusHelper.onSheetClosed(StateChangeReason.SWIPE);
        // Verify that the focus was restored.
        verify(mWebContentsAccessibility, times(1)).restoreFocus();
        // Verify that BottomSheetFocusHelper removed itself as an observer of the BottomSheet.
        verify(mBottomSheetController, times(1)).removeObserver(mBottomSheetFocusHelper);
    }
}
