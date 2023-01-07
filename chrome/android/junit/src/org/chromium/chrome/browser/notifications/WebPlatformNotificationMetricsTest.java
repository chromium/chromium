// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Arrays;
import java.util.Collection;

/**
 * Tests for {@link WebPlatformNotificationMetrics}.
 *
 * They are parameterized on whether the notification click occurred on the action button or on the
 * body.
 */
@RunWith(Parameterized.class)
public class WebPlatformNotificationMetricsTest {
    @Mock
    private WebPlatformNotificationMetrics.Clock mClock;
    @Mock
    private WebPlatformNotificationMetrics.Recorder mRecorder;

    private WebPlatformNotificationMetrics mMetrics;
    private boolean mActionButtonClicked;

    @Parameterized.Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    public WebPlatformNotificationMetricsTest(boolean actionButtonClicked) {
        mActionButtonClicked = actionButtonClicked;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mMetrics = new WebPlatformNotificationMetrics(mClock, mRecorder);

        when(mClock.getTime()).thenReturn(1000L);
    }

    @Test
    public void recordsClick() {
        mMetrics.onNotificationClicked(mActionButtonClicked);

        assertActionRecorded(mActionButtonClicked ? "Notifications.WebPlatformV2.ActionButton.Click"
                                                  : "Notifications.WebPlatformV2.Body.Click");
        assertNumActions(1);
    }

    @Test
    public void recordsFocus() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onTabFocused();

        assertActionRecorded(mActionButtonClicked
                        ? "Notifications.WebPlatformV2.ActionButton.FocusActivity"
                        : "Notifications.WebPlatformV2.Body.FocusActivity");
        assertNumActions(2); // Click + FocusActivity.
    }

    @Test
    public void recordsNewActivity() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onNewTabLaunched();
        mMetrics.onTabFocused();

        assertActionRecorded(mActionButtonClicked
                        ? "Notifications.WebPlatformV2.ActionButton.NewActivity"
                        : "Notifications.WebPlatformV2.Body.NewActivity");
        assertNumActions(2); // Click + NewActivity.
    }

    @Test
    public void recordsClose() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onNotificationClosed();

        assertActionRecorded(mActionButtonClicked ? "Notifications.WebPlatformV2.ActionButton.Close"
                                                  : "Notifications.WebPlatformV2.Body.Close");
        assertNumActions(2); // Click + Close.
    }

    @Test
    public void recordsCloseAndFocus() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onNotificationClosed();

        assertActionRecorded(mActionButtonClicked ? "Notifications.WebPlatformV2.ActionButton.Close"
                                                  : "Notifications.WebPlatformV2.Body.Close");

        mMetrics.onTabFocused();

        assertActionRecorded(mActionButtonClicked
                        ? "Notifications.WebPlatformV2.ActionButton.FocusActivity"
                        : "Notifications.WebPlatformV2.Body.FocusActivity");
        assertNumActions(3); // Click + Close + FocusActivity.
    }

    @Test
    public void recordsAtMostOneClosePerNotification() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onNotificationClosed();
        mMetrics.onNotificationClosed();

        assertNumActions(2); // Click + Close.
    }

    @Test
    public void recordsCloseAfterFocus() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onTabFocused();
        mMetrics.onNotificationClosed();

        assertNumActions(3); // Click + FocusActivity + Close.
    }

    @Test
    public void recordsAtMostOneFocusPerNotification() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onTabFocused();
        mMetrics.onTabFocused();

        assertNumActions(2); // Click + Focus.
    }

    @Test
    public void recordsMultipleNotifications() {
        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onTabFocused();

        mMetrics.onNotificationClicked(mActionButtonClicked);
        mMetrics.onTabFocused();

        assertNumActions(4); // Click + Focus + Click + Focus.
    }

    @Test
    public void timeLimitOnClose() {
        mMetrics.onNotificationClicked(mActionButtonClicked);

        when(mClock.getTime()).thenReturn(12_000L);
        mMetrics.onNotificationClosed();

        assertNumActions(1); // Click.
    }

    @Test
    public void timeLimitOnFocus() {
        mMetrics.onNotificationClicked(mActionButtonClicked);

        when(mClock.getTime()).thenReturn(12_000L);
        mMetrics.onTabFocused();

        assertNumActions(1); // Click.
    }

    @Test
    public void recordsTimeToActivity() {
        mMetrics.onNotificationClicked(mActionButtonClicked);

        when(mClock.getTime()).thenReturn(3000L);
        mMetrics.onTabFocused();

        assertDurationRecorded(mActionButtonClicked
                        ? "Notifications.WebPlatformV2.ActionButton.TimeToActivity"
                        : "Notifications.WebPlatformV2.Body.TimeToActivity",
                2000);
    }

    @Test
    public void recordsTimeToClose() {
        mMetrics.onNotificationClicked(mActionButtonClicked);

        when(mClock.getTime()).thenReturn(4000L);
        mMetrics.onNotificationClosed();

        assertDurationRecorded(mActionButtonClicked
                        ? "Notifications.WebPlatformV2.ActionButton.TimeToClose"
                        : "Notifications.WebPlatformV2.Body.TimeToClose",
                3000);
    }

    private void assertActionRecorded(String action) {
        verify(mRecorder).recordAction(eq(action));
    }

    private void assertNumActions(int num) {
        verify(mRecorder, times(num)).recordAction(anyString());
    }

    private void assertDurationRecorded(String name, long ms) {
        verify(mRecorder).recordDuration(eq(name), eq(ms));
    }
}
