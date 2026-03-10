// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/** Unit tests for {@link ChromeFindsOptInCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ChromeFindsOptInCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;

    private ChromeFindsOptInCoordinator mCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);

        mCoordinator =
                new ChromeFindsOptInCoordinator(
                        mActivity, mBottomSheetController, mSnackbarManager);
    }

    @Test
    public void testShowBottomSheet() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsMetrics.ChromeFindsOptInEvent.SHOWN);
        mCoordinator.showBottomSheet();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
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
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsMetrics.ChromeFindsOptInEvent.ACCEPTED_FIRST_TIME);
        // Simulate positive button click
        mCoordinator.onOptInAccepted();

        // Verify channel is created
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify snackbar is shown
        verify(mSnackbarManager).showSnackbar(any());
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
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsMetrics.ChromeFindsOptInEvent.ACCEPTED_FIRST_TIME);
        // Simulate positive button click
        mCoordinator.onOptInAccepted();

        // Verify channel is created
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify notification settings were launched instead of showing a snackbar.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());

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
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsMetrics.ChromeFindsOptInEvent.ACCEPTED_RE_OPT_IN);

        mCoordinator.onOptInAccepted();

        // Verify notification settings were launched.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());

        watcher.assertExpected();
    }

    @Test
    public void testOnOptInDeclined() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsMetrics.ChromeFindsOptInEvent.DECLINED);
        mCoordinator.onOptInDeclined();

        // Verify channel is created and disabled
        verify(mNotificationManagerProxy).createNotificationChannel(any());
        // Verify preference is set
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CHROME_FINDS_OPT_IN_PROMO_DECLINED, false));
        watcher.assertExpected();
    }
}
