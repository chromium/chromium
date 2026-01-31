// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.junit.Assert.assertNull;

import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
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

    private ChromeTabbedActivity mActivity;
    private MonotonicObservableSupplier<Profile> mProfileSupplier;
    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private TabBottomSheetManager mManager;
    private TabBottomSheetToolbar mToolbar;
    private View mFusebox;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();

        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileSupplier =
                            new MonotonicObservableSupplier<Profile>() {
                                @Override
                                public @Nullable Profile addObserver(
                                        Callback<Profile> obs, int behavior) {
                                    return null;
                                }

                                @Override
                                public void removeObserver(Callback<Profile> obs) {}

                                @Override
                                public int getObserverCount() {
                                    return 0;
                                }

                                @Override
                                public Profile get() {
                                    return mActivity
                                            .getProfileProviderSupplier()
                                            .get()
                                            .getOriginalProfile();
                                }
                            };
                    mWindowAndroid = mActivity.getWindowAndroid();
                    mBottomSheetController =
                            mActivity.getRootUiCoordinatorForTesting().getBottomSheetController();

                    mToolbar = new TabBottomSheetSimpleToolbar(mActivity);
                    mFusebox = new FrameLayout(mActivity);

                    createManager();
                });
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mManager.destroy());
        }
    }

    private void createManager() {
        mManager =
                new TabBottomSheetManager(
                        mActivity,
                        mProfileSupplier,
                        mWindowAndroid,
                        mActivity.getLifecycleDispatcher(),
                        mActivity.getSnackbarManager(),
                        mBottomSheetController);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
    public void testTryToShowBottomSheet_FeatureDisabled_NoBottomSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.tryToShowBottomSheet(
                            /* shouldShowToolbar= */ true, /* shouldShowFusebox */ true);
                });
        assertNull(mManager.getTabBottomSheetCoordinatorForTesting());
    }
}
