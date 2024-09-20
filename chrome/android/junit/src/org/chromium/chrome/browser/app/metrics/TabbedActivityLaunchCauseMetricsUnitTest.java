// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.metrics;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.nfc.NfcAdapter;
import android.provider.Browser;
import android.speech.RecognizerResultsIntent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.components.webapps.ShortcutSource;

/** Unit tests for TabbedActivityLaunchCauseMetrics. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabbedActivityLaunchCauseMetricsUnitTest {
    @Mock private Activity mActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        LaunchCauseMetrics.resetForTests();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
    }

    private static int histogramCountForValue(int value) {
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    @Test
    public void testOpenInBrowserMetrics() throws Throwable {
        int count =
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("about:blank"));
        intent.putExtra(IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, true);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        // Tests the case where Chrome is backgrounded either by the intent picker, or by
        // cross-channel Open In Browser.
        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU, launchCause);
        Assert.assertEquals(
                count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));

        // Resume a different ChromeActivity to make it look like we're transitioning between
        // ChromeActivitys.
        ChromeTabbedActivity chromeActivity = new ChromeTabbedActivity();
        ApplicationStatus.onStateChangeForTesting(chromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(chromeActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(chromeActivity, ActivityState.RESUMED);

        // Ensures we record this metric even when Chrome has already recorded a launch.
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU, launchCause);
        Assert.assertEquals(
                count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));

        // Ensures we don't record this metric again without a new Intent having been received.
        launchCause = metrics.recordLaunchCause();
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OTHER, launchCause);
        Assert.assertEquals(
                count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));

        // Ensures other metrics still aren't recorded when Chrome has already recorded a launch.
        int total =
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM);
        intent.putExtra(IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, false);
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OTHER, launchCause);
        Assert.assertEquals(
                total,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));
    }

    @Test
    public void testVoiceSearchResultsMetrics() throws Throwable {
        int count =
                histogramCountForValue(
                        LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT);
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(
                LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT, launchCause);
        Assert.assertEquals(
                count,
                histogramCountForValue(
                        LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT));

        // Ensures we don't record this metric when Chrome has already recorded a launch.
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        Assert.assertEquals(
                LaunchCauseMetrics.LaunchCause.OTHER, launchCause);
        Assert.assertEquals(
                count,
                histogramCountForValue(
                        LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT));
    }

    @Test
    public void testBringToFrontNotification() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION);
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        1, IntentHandler.BringToFrontSource.NOTIFICATION);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.NOTIFICATION, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION));

        // Resume a different ChromeActivity to make it look like we're transitioning between
        // ChromeActivitys.
        ChromeTabbedActivity chromeActivity = new ChromeTabbedActivity();
        ApplicationStatus.onStateChangeForTesting(chromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(chromeActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(chromeActivity, ActivityState.RESUMED);

        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.NOTIFICATION, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION));
    }

    @Test
    public void testBringToFrontSearch() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET);
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        1, IntentHandler.BringToFrontSource.SEARCH_ACTIVITY);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_WIDGET));
    }

    @Test
    public void testBringToFrontActiviteTab() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION);
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        1, IntentHandler.BringToFrontSource.ACTIVATE_TAB);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.NOTIFICATION, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.NOTIFICATION));
    }

    @Test
    public void testExternalViewIntent() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.EXTERNAL_VIEW_INTENT);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.EXTERNAL_VIEW_INTENT, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.EXTERNAL_VIEW_INTENT));
    }

    @Test
    public void testChromeViewIntent() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER_CHROME);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID,
                ContextUtils.getApplicationContext().getPackageName());
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OTHER_CHROME, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER_CHROME));
    }

    @Test
    public void testOtherChromeIntent() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER_CHROME);
        Intent intent = new Intent();
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID,
                ContextUtils.getApplicationContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OTHER_CHROME, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.OTHER_CHROME));
    }

    @Test
    public void testHomescreenShortcut() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_SHORTCUT);
        Intent intent =
                ShortcutHelper.createShortcutIntent(
                        "about:blank", "id", ShortcutSource.ADD_TO_HOMESCREEN_SHORTCUT);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_SHORTCUT, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.HOME_SCREEN_SHORTCUT));
    }

    @Test
    public void testShareIntent() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.SHARE_INTENT);
        Intent intent = new Intent(Intent.ACTION_SEND, Uri.parse("https://example.com"));
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.SHARE_INTENT, launchCause);
        Assert.assertEquals(
                count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.SHARE_INTENT));
    }

    @Test
    public void testNfcViewIntent() throws Throwable {
        int count = histogramCountForValue(LaunchCauseMetrics.LaunchCause.NFC);
        Intent intent =
                new Intent(NfcAdapter.ACTION_NDEF_DISCOVERED, Uri.parse("https://example.com"));
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.NFC, launchCause);
        Assert.assertEquals(count, histogramCountForValue(LaunchCauseMetrics.LaunchCause.NFC));
    }
}
