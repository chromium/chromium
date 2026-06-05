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
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_ACCEPTED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.res.Configuration;
import android.os.Looper;
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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.notifications.tips.TipsOptInCoordinator.TipsOptInSheetContent;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Unit tests for {@link TipsOptInCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED)
public class TipsOptInCoordinatorUnitTest {
    private static final int NARROW_SCREEN_WIDTH_DP = 300;
    private static final int WIDE_SCREEN_WIDTH_DP = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;

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
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
        mTipsOptInCoordinator =
                new TipsOptInCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mSnackbarManager,
                        mSharedPreferenceManager);
        mView = mTipsOptInCoordinator.getViewForTesting();
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetContent = mTipsOptInCoordinator.getBottomSheetContentForTesting();
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
                        "Notifications.Tips.OptInPromo.EventType2",
                        TipsOptInCoordinator.OptInPromoEventType.SHOWN);

        assertEquals(
                -1,
                mSharedPreferenceManager.readInt(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, -1));
        assertEquals(
                -1L,
                mSharedPreferenceManager.readLong(
                        TIPS_NOTIFICATIONS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP, -1L));

        mTipsOptInCoordinator.showBottomSheet();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        assertEquals(
                1,
                mSharedPreferenceManager.readInt(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, -1));
        assertTrue(
                mSharedPreferenceManager.readLong(
                                TIPS_NOTIFICATIONS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP, -1L)
                        > 0);

        // Verify shown pref is still false.
        assertFalse(
                mSharedPreferenceManager.readBoolean(
                        TIPS_NOTIFICATIONS_OPT_IN_PROMO_ACCEPTED, false));

        histogramWatcher.assertExpected();
    }

    @Test
    public void testPositiveButtonClick() {
        assertFalse(
                mSharedPreferenceManager.readBoolean(
                        TIPS_NOTIFICATIONS_OPT_IN_PROMO_ACCEPTED, false));

        mView.findViewById(R.id.opt_in_positive_button).performClick();

        assertTrue(
                mSharedPreferenceManager.readBoolean(
                        TIPS_NOTIFICATIONS_OPT_IN_PROMO_ACCEPTED, false));
    }

    @Test
    public void testSheetContent_onBackPressed() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType2",
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
                                "Notifications.Tips.OptInPromo.EventType2",
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
    public void testOnOptInAccepted() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType2",
                        TipsOptInCoordinator.OptInPromoEventType.ACCEPTED);

        mTipsOptInCoordinator.onOptInAccepted();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mBottomSheetController).hideContent(any(), eq(true));

        // Simulate sheet closure to trigger snackbar.
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetClosed(StateChangeReason.NONE);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        // Verify snackbar was shown.
        verify(mSnackbarManager).showSnackbar(any());

        histogramWatcher.assertExpected();

        // Verify channel exists and is enabled.
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChannelId.TIPS_V2,
                        (channel) -> {
                            assertTrue(channel != null);
                            assertEquals(
                                    NotificationManager.IMPORTANCE_DEFAULT,
                                    channel.getImportance());
                        });
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void testOnOptInDeclined() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType2",
                        TipsOptInCoordinator.OptInPromoEventType.DECLINED);

        mTipsOptInCoordinator.onOptInDeclined();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mBottomSheetController).hideContent(any(), eq(true));
        histogramWatcher.assertExpected();

        // Verify channel exists and is disabled.
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChannelId.TIPS_V2,
                        (channel) -> {
                            assertTrue(channel != null);
                            assertEquals(
                                    NotificationManager.IMPORTANCE_NONE, channel.getImportance());
                        });
        Shadows.shadowOf(Looper.getMainLooper()).idle();
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
