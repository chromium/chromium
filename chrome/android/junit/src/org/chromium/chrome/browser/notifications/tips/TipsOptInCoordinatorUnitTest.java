// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.notifications.tips.TipsUtils.LOGO_IMAGE_MAX_WIDTH_RATIO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.tips.TipsOptInCoordinator.TipsOptInSheetContent;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Unit tests for {@link TipsOptInCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsOptInCoordinatorUnitTest {
    private static final int NARROW_SCREEN_WIDTH_DP = 300;
    private static final int WIDE_SCREEN_WIDTH_DP = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;

    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private Activity mActivity;
    private TipsOptInCoordinator mTipsOptInCoordinator;
    private TipsOptInSheetContent mBottomSheetContent;
    private SharedPreferencesManager mSharedPreferenceManager;
    private View mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTipsOptInCoordinator = new TipsOptInCoordinator(mActivity, mBottomSheetController);
        mView = mTipsOptInCoordinator.getViewForTesting();
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetContent = mTipsOptInCoordinator.getBottomSheetContentForTesting();
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
    }

    @SmallTest
    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType",
                        TipsOptInCoordinator.OptInPromoEventType.SHOWN);

        assertFalse(
                mSharedPreferenceManager.readBoolean(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, false));
        mTipsOptInCoordinator.showBottomSheet();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        assertTrue(
                mSharedPreferenceManager.readBoolean(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, false));

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSheetContent_onBackPressed() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType",
                        TipsOptInCoordinator.OptInPromoEventType.IGNORED);

        mBottomSheetContent.onBackPressed();
        verify(mBottomSheetController).hideContent(any(), eq(true));

        histogramWatcher.assertExpected();
    }

    @Test
    public void testBottomSheetObserver() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Notifications.Tips.OptInPromo.EventType",
                                TipsOptInCoordinator.OptInPromoEventType.IGNORED,
                                2)
                        .build();

        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetOpened(StateChangeReason.NONE);

        observer.onSheetClosed(StateChangeReason.TAP_SCRIM);
        verify(mBottomSheetController).removeObserver(eq(observer));

        clearInvocations(mBottomSheetController);

        observer.onSheetClosed(StateChangeReason.SWIPE);
        verify(mBottomSheetController).removeObserver(eq(observer));

        histogramWatcher.assertExpected();
    }

    @Test
    public void testConfigurationChangeScalesImageLogo() {
        // Set up a scenario where context configuration and parameter configuration differ
        // to verify that the parameter configuration is used.

        // Default context configuration should be narrow screen (phone).
        // Assert that the width is match parent for portrait phones.
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT,
                mView.findViewById(R.id.opt_in_logo).getLayoutParams().width);

        // Create a configuration parameter with wide screen (tablet)
        Configuration tabletConfig = new Configuration();
        tabletConfig.screenWidthDp = WIDE_SCREEN_WIDTH_DP;
        tabletConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;

        // Simulate configuration change with tablet config parameter
        // while context still has phone config.
        mTipsOptInCoordinator.triggerConfigurationChangeForTesting(tabletConfig);

        // Should now scale (tablet behavior) - proving it uses parameter config.
        assertEquals(
                Math.round(
                        ViewUtils.dpToPx(mActivity, WIDE_SCREEN_WIDTH_DP)
                                * LOGO_IMAGE_MAX_WIDTH_RATIO),
                mView.findViewById(R.id.opt_in_logo).getLayoutParams().width,
                MathUtils.EPSILON);

        // Create another configuration parameter with narrow screen (phone).
        Configuration phoneConfig = new Configuration();
        phoneConfig.screenWidthDp = NARROW_SCREEN_WIDTH_DP;
        phoneConfig.orientation = Configuration.ORIENTATION_PORTRAIT;

        // Simulate configuration change back to phone config.
        mTipsOptInCoordinator.triggerConfigurationChangeForTesting(phoneConfig);

        // Should now be match parent (phone behavior).
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT,
                mView.findViewById(R.id.opt_in_logo).getLayoutParams().width);
    }
}
