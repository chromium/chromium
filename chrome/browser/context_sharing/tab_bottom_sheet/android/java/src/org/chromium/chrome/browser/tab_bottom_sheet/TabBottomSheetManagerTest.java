// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager.NativeInterfaceDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Instrumentation tests for {@link TabBottomSheetManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabBottomSheetManagerTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private CoBrowseViews mCoBrowseViews;
    private ChromeTabbedActivity mActivity;
    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private TabBottomSheetManager mManager;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();

        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid = mActivity.getWindowAndroid();
                    mBottomSheetController =
                            mActivity.getRootUiCoordinatorForTesting().getBottomSheetController();
                    mCoBrowseViews = new CoBrowseViews(mActivity, null, null, null);
                    mManager = new TabBottomSheetManager(mWindowAndroid, mBottomSheetController);
                });
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mManager.destroy());
        }
    }

    @Test
    @SmallTest
    public void testTryToShowBottomSheet_Success_NativeInterfaceDelegateRegistered() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            NativeInterfaceDelegate.getInstance(), mCoBrowseViews);
                });
        assertEquals(
                mManager.getNativeInterfaceDelegateForTesting(),
                NativeInterfaceDelegate.getInstance());
    }
}
