// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
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

    private static final int REQUEST_ID = 0;

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
                    NonNullObservableSupplier<Profile> profileSupplier =
                            ObservableSuppliers.createNonNull(
                                    mActivity
                                            .getProfileProviderSupplier()
                                            .get()
                                            .getOriginalProfile());
                    mWindowAndroid = mActivity.getWindowAndroid();
                    mBottomSheetController =
                            mActivity.getRootUiCoordinatorForTesting().getBottomSheetController();

                    mManager =
                            new TabBottomSheetManager(
                                    mActivity,
                                    /* fuseboxConfig= */ null,
                                    profileSupplier,
                                    mWindowAndroid,
                                    mActivity.getLifecycleDispatcher(),
                                    mActivity.getSnackbarManager(),
                                    mBottomSheetController);
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
    @DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
    public void testTryToShowBottomSheet_FeatureDisabled_NoBottomSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            NativeInterfaceDelegate.getInstance(),
                            /* shouldShowToolbar= */ true,
                            /* shouldShowFusebox= */ true);
                });
        assertNull(mManager.getTabBottomSheetCoordinatorForTesting());
    }
}
