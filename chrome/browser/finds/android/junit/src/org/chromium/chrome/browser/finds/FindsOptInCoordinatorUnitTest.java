// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.content.res.Configuration;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.ButtonCompat;

/** Unit tests for {@link FindsOptInCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class FindsOptInCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;

    private FindsOptInCoordinator mCoordinator;
    private Activity mActivity;
    private BottomSheetObserver mBottomSheetObserver;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        UserPrefs.setPrefServiceForTesting(mPrefService);

        NotificationProxyUtils.setNotificationEnabledForTest(true);

        mCoordinator =
                new FindsOptInCoordinator(
                        mActivity, mProfile, mBottomSheetController, mSnackbarManager);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observerCaptor.capture());
        mBottomSheetObserver = observerCaptor.getValue();
    }

    private void simulateSheetClosed(@StateChangeReason int reason) {
        doReturn(mCoordinator.getSheetContentForTesting())
                .when(mBottomSheetController)
                .getCurrentSheetContent();
        mBottomSheetObserver.onSheetClosed(reason);
        assertEquals(
                FindsOptInCoordinator.FindsOptInUserInteraction.DISMISSED,
                mCoordinator.getUserInteractionTypeForTesting());
    }

    @Test
    public void testShowBottomSheet() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsMetrics.FindsOptInEvent.SHOWN);
        mCoordinator.showBottomSheet();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        verify(mPrefService).setInteger(FindsUtils.FINDS_OPT_IN_PROMO_SHOWN_COUNT, 1);
        verify(mPrefService)
                .setLong(eq(FindsUtils.FINDS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP), anyLong());
        watcher.assertExpected();
    }

    @Test
    public void testOnOptInAccepted_FirstTime_AppNotificationsEnabled() {
        // Mock app-level notifications enabled.
        NotificationProxyUtils.setNotificationEnabledForTest(true);

        // Mock channel doesn't exist.
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM,
                        FindsMetrics.FindsOptInEvent.ACCEPTED_FIRST_TIME);
        // Simulate positive button click.
        ButtonCompat positiveButton =
                mCoordinator.getContentViewForTesting().findViewById(R.id.opt_in_positive_button);
        positiveButton.performClick();

        assertEquals(
                FindsOptInCoordinator.FindsOptInUserInteraction.ACCEPTED,
                mCoordinator.getUserInteractionTypeForTesting());

        // Mock channel query to return an enabled channel, simulating successful creation.
        NotificationChannel mockEnabledChannel =
                new NotificationChannel(
                        ChannelId.CHROME_FINDS, "Finds", NotificationManager.IMPORTANCE_DEFAULT);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(mockEnabledChannel);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        // Simulate sheet closure as a result of opt-in.
        simulateSheetClosed(StateChangeReason.NONE);

        // Verify channel is created
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify snackbar is shown
        verify(mSnackbarManager).showSnackbar(any());
        // Verify preference is set via UserPrefs
        verify(mPrefService).setBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
        watcher.assertExpected();
    }

    @Test
    public void testOnOptInAccepted_FirstTime_AppNotificationsDisabled() {
        // Mock app-level notifications disabled.
        NotificationProxyUtils.setNotificationEnabledForTest(false);

        // Mock channel doesn't exist.
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM,
                        FindsMetrics.FindsOptInEvent.ACCEPTED_FIRST_TIME);
        // Simulate positive button click.
        ButtonCompat positiveButton =
                mCoordinator.getContentViewForTesting().findViewById(R.id.opt_in_positive_button);
        positiveButton.performClick();

        assertEquals(
                FindsOptInCoordinator.FindsOptInUserInteraction.ACCEPTED,
                mCoordinator.getUserInteractionTypeForTesting());

        // Simulate sheet closure as a result of opt-in.
        simulateSheetClosed(StateChangeReason.NONE);

        // Verify channel is created
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify notification settings were launched instead of showing a snackbar.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
        // Verify snackbar is not shown.
        verify(mSnackbarManager, never()).showSnackbar(any());
        // Verify preference is set via UserPrefs
        verify(mPrefService).setBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
        watcher.assertExpected();
    }

    @Test
    public void testOnOptInAccepted_ReOptIn() {
        // Mock notifications are enabled for the app.
        NotificationProxyUtils.setNotificationEnabledForTest(true);

        // Mock channel exists but is disabled.
        NotificationChannel channel =
                new NotificationChannel(
                        ChannelId.CHROME_FINDS, "Finds", NotificationManager.IMPORTANCE_NONE);
        doAnswer(
                        invocation -> {
                            Callback<NotificationChannel> callback = invocation.getArgument(1);
                            callback.onResult(channel);
                            return null;
                        })
                .when(mNotificationManagerProxy)
                .getNotificationChannel(eq(ChannelId.CHROME_FINDS), any());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM,
                        FindsMetrics.FindsOptInEvent.ACCEPTED_RE_OPT_IN);

        // Simulate positive button click.
        ButtonCompat positiveButton =
                mCoordinator.getContentViewForTesting().findViewById(R.id.opt_in_positive_button);
        positiveButton.performClick();

        assertEquals(
                FindsOptInCoordinator.FindsOptInUserInteraction.ACCEPTED,
                mCoordinator.getUserInteractionTypeForTesting());

        // Simulate sheet closure as a result of opt-in.
        simulateSheetClosed(StateChangeReason.NONE);

        // Verify notification settings were launched.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
        // Verify snackbar is not shown.
        verify(mSnackbarManager, never()).showSnackbar(any());
        // Verify preference is set via UserPrefs
        verify(mPrefService).setBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
        watcher.assertExpected();
    }

    @Test
    public void testOnOptInDeclined() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsMetrics.FindsOptInEvent.DECLINED);

        // Simulate negative button click.
        ButtonCompat negativeButton =
                mCoordinator.getContentViewForTesting().findViewById(R.id.opt_in_negative_button);
        negativeButton.performClick();

        assertEquals(
                FindsOptInCoordinator.FindsOptInUserInteraction.DECLINED,
                mCoordinator.getUserInteractionTypeForTesting());

        // Simulate sheet closure as a result of decline.
        simulateSheetClosed(StateChangeReason.NONE);

        // Verify channel is created and disabled
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify snackbar is not shown.
        verify(mSnackbarManager, never()).showSnackbar(any());
        // Verify preference is set via UserPrefs
        verify(mPrefService).setBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
        watcher.assertExpected();
    }

    @Test
    public void testScaleBottomSheetLottieAnimationByHeight_TargetWidthSmallerThanMaxWidth() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        Configuration configuration = new Configuration();
        configuration.screenHeightDp = 1000;
        configuration.screenWidthDp = 1000; // Large width so targetWidth < maxWidth

        mCoordinator.scaleBottomSheetLottieAnimationByHeight(configuration);

        View animationView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.finds_opt_in_lottie_animation);
        ViewGroup.LayoutParams layoutParams = animationView.getLayoutParams();

        int screenHeightPixels = ViewUtils.dpToPx(mActivity, 1000);
        int maxHeight =
                Math.round(screenHeightPixels * FindsOptInCoordinator.LOTTIE_MAX_HEIGHT_RATIO);
        int expectedWidth =
                Math.round(maxHeight * FindsOptInCoordinator.LOTTIE_INTRINSIC_ASPECT_RATIO);

        assertEquals(expectedWidth, layoutParams.width);
    }

    @Test
    public void testScaleBottomSheetLottieAnimationByHeight_TargetWidthLimitedByMaxWidth() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        Configuration configuration = new Configuration();
        configuration.screenHeightDp = 1000;
        configuration.screenWidthDp = 100; // Small width so targetWidth > maxWidth

        mCoordinator.scaleBottomSheetLottieAnimationByHeight(configuration);

        View animationView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.finds_opt_in_lottie_animation);
        ViewGroup.LayoutParams layoutParams = animationView.getLayoutParams();

        int screenWidthPixels = ViewUtils.dpToPx(mActivity, 100);
        int horizontalMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.finds_opt_in_bottom_sheet_horizontal_margin);
        int maxWidth = screenWidthPixels - (horizontalMargin * 2);

        assertEquals(maxWidth, layoutParams.width);
    }

    @Test
    public void testScaleBottomSheetLottieAnimationByHeight_TargetWidthLimitedByMaxSheetWidth() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);

        // Simulate where the max sheet width is artificially constrained.
        int fakeMaxSheetWidthPx = 500;
        doReturn(fakeMaxSheetWidthPx).when(mBottomSheetController).getMaxSheetWidth();

        Configuration configuration = new Configuration();
        // Ensure that screen height is not limiting the width calculation.
        configuration.screenHeightDp = 2000;
        configuration.screenWidthDp = 1000;

        mCoordinator.scaleBottomSheetLottieAnimationByHeight(configuration);

        View animationView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.finds_opt_in_lottie_animation);
        ViewGroup.LayoutParams layoutParams = animationView.getLayoutParams();

        int horizontalMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.finds_opt_in_bottom_sheet_horizontal_margin);
        int expectedMaxWidth = fakeMaxSheetWidthPx - (horizontalMargin * 2);
        assertEquals(expectedMaxWidth, layoutParams.width);
    }

    @Test
    public void testRecordOptInDismissed() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);

        // Simulate sheet opening.
        doReturn(mCoordinator.getSheetContentForTesting())
                .when(mBottomSheetController)
                .getCurrentSheetContent();
        mBottomSheetObserver.onSheetOpened(StateChangeReason.NONE);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsMetrics.FindsOptInEvent.DISMISSED);

        // Simulate sheet dismissal.
        assertEquals(
                FindsOptInCoordinator.FindsOptInUserInteraction.DISMISSED,
                mCoordinator.getUserInteractionTypeForTesting());
        simulateSheetClosed(StateChangeReason.SWIPE);

        watcher.assertExpected();
        // The snackbar should not be shown if the user didn't opt-in.
        verify(mSnackbarManager, never()).showSnackbar(any());
    }
}
