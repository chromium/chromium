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
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.provider.Settings;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
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
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

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

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        UserPrefs.setPrefServiceForTesting(mPrefService);

        mCoordinator =
                new FindsOptInCoordinator(
                        mActivity, mProfile, mBottomSheetController, mSnackbarManager);
    }

    @Test
    public void testShowBottomSheet() {
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
        // Simulate positive button click
        mCoordinator.onOptInAccepted();

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
        // Simulate positive button click
        mCoordinator.onOptInAccepted();

        // Verify channel is created
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify notification settings were launched instead of showing a snackbar.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
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

        mCoordinator.onOptInAccepted();

        // Verify notification settings were launched.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
        // Verify preference is set via UserPrefs
        verify(mPrefService).setBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
        watcher.assertExpected();
    }

    @Test
    public void testOnOptInDeclined() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsMetrics.FindsOptInEvent.DECLINED);
        mCoordinator.onOptInDeclined();

        // Verify channel is created and disabled
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify preference is set via UserPrefs
        verify(mPrefService).setBoolean(FindsUtils.FINDS_OPT_IN_PROMO_USER_INTERACTED, true);
        watcher.assertExpected();
    }
}
