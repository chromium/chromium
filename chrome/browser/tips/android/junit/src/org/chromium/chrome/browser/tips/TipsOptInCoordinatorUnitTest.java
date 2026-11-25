// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_ACCEPTED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT;
import static org.chromium.chrome.browser.tips.TipsUtils.LOGO_IMAGE_MAX_WIDTH_RATIO;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.res.Configuration;
import android.os.Looper;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;

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

import org.chromium.base.MathUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tips.TipsOptInCoordinator.TipsOptInSheetContent;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Unit tests for {@link TipsOptInCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED)
public class TipsOptInCoordinatorUnitTest {
    private static final int NARROW_SCREEN_WIDTH_DP = 300;
    private static final int WIDE_SCREEN_WIDTH_DP = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;

    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private Activity mActivity;
    private TipsOptInCoordinator mTipsOptInCoordinator;
    private TipsOptInSheetContent mBottomSheetContent;
    private SharedPreferencesManager mSharedPreferenceManager;
    private View mView;

    @Before
    public void setUp() {
        ChannelsInitializer.resetForTesting();
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mTipsOptInCoordinator =
                new TipsOptInCoordinator(
                        mActivity,
                        mBottomSheetController,
                        mSnackbarManager,
                        mSharedPreferenceManager,
                        mTracker);
        mView = mTipsOptInCoordinator.getViewForTesting();
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetContent = mTipsOptInCoordinator.getBottomSheetContentForTesting();
    }

    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();
    }

    @Test
    public void testShowBottomSheet() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType3",
                        TipsOptInCoordinator.OptInPromoEventType.SHOWN);

        assertTrue(mTipsOptInCoordinator.showBottomSheet());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        // Nothing is counted until the sheet opens.
        assertEquals(
                -1,
                mSharedPreferenceManager.readInt(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, -1));
        assertEquals(
                -1L,
                mSharedPreferenceManager.readLong(
                        TIPS_NOTIFICATIONS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP, -1L));

        openSheet();

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
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.IGNORED)
                        .build();

        openSheet();
        mBottomSheetContent.onBackPressed();
        verify(mBottomSheetController)
                .hideContent(any(), eq(true), eq(StateChangeReason.BACK_PRESS));
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSheetClosedByUser_recordsIgnored() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.IGNORED)
                        .build();

        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        openSheet();
        observer.onSheetClosed(StateChangeReason.TAP_SCRIM);

        // Nothing else is recorded once there's an outcome.
        observer.onSheetOpened(StateChangeReason.NONE);
        observer.onSheetClosed(StateChangeReason.SWIPE);
        mTipsOptInCoordinator.destroy();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSheetHiddenBySystem_recordsClosedBySystemOnDestroy() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType3",
                        TipsOptInCoordinator.OptInPromoEventType.SHOWN);
        openSheet();
        // The sheet may open again, so there's no outcome yet.
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.NONE);
        shownWatcher.assertExpected();

        HistogramWatcher closedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType3",
                        TipsOptInCoordinator.OptInPromoEventType.CLOSED_BY_SYSTEM);
        mTipsOptInCoordinator.destroy();
        closedWatcher.assertExpected();
    }

    @Test
    public void testSheetHiddenBySystemAndShownAgain_recordsOneShowAndOutcome() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.IGNORED)
                        .build();

        // E.g. the omnibox is focused and then loses focus.
        openSheet();
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.NONE);
        openSheet();
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);
        mTipsOptInCoordinator.destroy();

        assertEquals(
                1, mSharedPreferenceManager.readInt(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, 0));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOtherSheet_isIgnored() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Notifications.Tips.OptInPromo.EventType3")
                        .build();
        when(mBottomSheetController.getCurrentSheetContent())
                .thenReturn(mock(BottomSheetContent.class));

        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetOpened(StateChangeReason.NONE);
        observer.onSheetClosed(StateChangeReason.SWIPE);

        verify(mBottomSheetController, never()).removeObserver(any());
        verify(mTracker, never()).dismissed(any());
        assertFalse(mBottomSheetContent.getBackPressStateChangedSupplier().get());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroyWhileOpen_recordsClosedBySystem() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.CLOSED_BY_SYSTEM)
                        .build();

        openSheet();
        mTipsOptInCoordinator.destroy();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroyBeforeOpen_recordsNothing() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Notifications.Tips.OptInPromo.EventType3")
                        .build();

        mTipsOptInCoordinator.showBottomSheet();
        mTipsOptInCoordinator.destroy();

        verify(mBottomSheetController).removeObserver(mBottomSheetObserverCaptor.getValue());
        assertEquals(
                -1,
                mSharedPreferenceManager.readInt(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, -1));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnOptInAccepted() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.ACCEPTED)
                        .build();

        openSheet();
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
    public void testOnOptInAccepted_AppNotificationsDisabled_LaunchesSettings() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);

        mTipsOptInCoordinator.onOptInAccepted();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertEquals(
                Settings.ACTION_APP_NOTIFICATION_SETTINGS,
                Shadows.shadowOf(mActivity).getNextStartedActivity().getAction());
        verify(mBottomSheetController).hideContent(any(), eq(true));
    }

    @Test
    public void testOnOptInAccepted_ChannelDisabled_LaunchesChannelSettings() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        new ChannelsInitializer(
                        BaseNotificationManagerProxyFactory.create(),
                        ChromeChannelDefinitions.getInstance(),
                        mActivity.getResources())
                .ensureInitializedAndDisabled(ChannelId.TIPS_V2);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        mTipsOptInCoordinator.onOptInAccepted();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertEquals(
                Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS,
                Shadows.shadowOf(mActivity).getNextStartedActivity().getAction());
        verify(mBottomSheetController).hideContent(any(), eq(true));
    }

    @Test
    public void testOnOptInAccepted_DestroyedBeforeComplete_DoesNotLaunchSettings() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);

        mTipsOptInCoordinator.onOptInAccepted();
        mTipsOptInCoordinator.destroy();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivity());
        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean());
    }

    @Test
    public void testOnOptInDeclined() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Notifications.Tips.OptInPromo.EventType3",
                        TipsOptInCoordinator.OptInPromoEventType.DECLINED);

        mTipsOptInCoordinator.onOptInDeclined();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mBottomSheetController).hideContent(any(), eq(true));
        histogramWatcher.assertExpected();
        // Decliners remain eligible for the passive Magic Stack card.
        assertTrue(mSharedPreferenceManager.readBoolean(TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, false));

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
    public void testOnOptInAccepted_DuplicateActions_Ignored() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.ACCEPTED)
                        .build();

        openSheet();
        mTipsOptInCoordinator.onOptInAccepted();
        mTipsOptInCoordinator.onOptInAccepted();
        mTipsOptInCoordinator.onOptInDeclined();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertEquals(
                Settings.ACTION_APP_NOTIFICATION_SETTINGS,
                Shadows.shadowOf(mActivity).getNextStartedActivity().getAction());
        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivity());
        verify(mBottomSheetController).hideContent(any(), eq(true));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnOptInDeclined_DuplicateActions_Ignored() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.DECLINED)
                        .build();

        openSheet();
        mTipsOptInCoordinator.onOptInDeclined();
        mTipsOptInCoordinator.onOptInDeclined();
        mTipsOptInCoordinator.onOptInAccepted();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mBottomSheetController).hideContent(any(), eq(true));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testButtonActionAfterSheetClosed_Ignored() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Notifications.Tips.OptInPromo.EventType3",
                                TipsOptInCoordinator.OptInPromoEventType.SHOWN,
                                TipsOptInCoordinator.OptInPromoEventType.IGNORED)
                        .build();

        openSheet();
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);
        mTipsOptInCoordinator.onOptInAccepted();
        mTipsOptInCoordinator.onOptInDeclined();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean());
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

    @Test
    public void testTrackerDismissedOnSheetClosed() {
        mTipsOptInCoordinator.showBottomSheet();
        openSheet();
        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.TAP_SCRIM);
        verify(mTracker).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);
    }

    @Test
    public void testTrackerNotDismissedWhileHidden() {
        openSheet();
        // E.g. the omnibox is focused. The sheet may open again, so the tracker stays blocked.
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.NONE);
        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);

        mTipsOptInCoordinator.destroy();
        verify(mTracker).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);
    }

    @Test
    public void testTrackerDismissedWhenReplacedWhileHidden() {
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        openSheet();
        observer.onSheetClosed(StateChangeReason.NONE);

        observer.onSheetContentChanged(mBottomSheetContent);
        verify(mTracker, never()).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);

        observer.onSheetContentChanged(mock(BottomSheetContent.class));
        verify(mTracker).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);
    }

    @Test
    public void testTrackerDismissedWhenShowBottomSheetFails() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);

        assertFalse(mTipsOptInCoordinator.showBottomSheet());
        verify(mTracker).dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);
    }

    private void openSheet() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);
    }
}
