// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.media.AudioManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.media.AudioBecomingNoisyReceiver;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content.browser.MediaSessionImplJni;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.test.mock.MockWebContents;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link MediaSessionTabHelper} lazy-initialization and cleanup. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/522397811): Add tests with ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS enabled once
// the feature is implemented.
@DisableFeatures(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS)
public class MediaSessionTabHelperTest {
    private Tab mTab;
    private MockWebContents mWebContents;
    private MediaSessionImpl.Natives mMediaSessionImplJniMock;
    private MediaSessionImpl mMediaSession;
    private MediaSessionTabHelper mHelper;

    @Before
    public void setUp() {
        mTab = mock(Tab.class);
        mWebContents = mock(MockWebContents.class);
        mMediaSessionImplJniMock = mock(MediaSessionImpl.Natives.class);
        mMediaSession = mock(MediaSessionImpl.class);

        MediaSessionImplJni.setInstanceForTesting(mMediaSessionImplJniMock);
        when(mMediaSessionImplJniMock.getMediaSessionFromWebContents(mWebContents))
                .thenReturn(mMediaSession);
    }

    @After
    public void tearDown() {
        RobolectricUtil.runAllBackgroundAndUi();
        AudioBecomingNoisyReceiver.resetForTesting();
    }

    @Test
    public void testLazyInitialization() {
        // Start with null WebContents (simulating frozen tab)
        when(mTab.getWebContents()).thenReturn(null);

        mHelper = new MediaSessionTabHelper(mTab);

        // Helper should exist, but inner MediaSessionHelper should be null
        assertNotNull(mHelper);
        assertNull(mHelper.mMediaSessionHelper);

        // Simulate WebContents being set (tab restored)
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mHelper.mTabObserver.onContentChanged(mTab);

        // Inner helper should now be initialized
        assertNotNull(mHelper.mMediaSessionHelper);
    }

    @Test
    public void testCleanupOnFreeze() {
        // Start with active WebContents
        when(mTab.getWebContents()).thenReturn(mWebContents);

        mHelper = new MediaSessionTabHelper(mTab);

        // Inner helper should be initialized
        assertNotNull(mHelper.mMediaSessionHelper);

        // Simulate WebContents being cleared (tab frozen)
        when(mTab.getWebContents()).thenReturn(null);
        mHelper.mTabObserver.onContentChanged(mTab);

        // Inner helper should be destroyed and set to null
        assertNull(mHelper.mMediaSessionHelper);
    }

    @Test
    public void testAudioBecomingNoisy_NonDesktopDevice_PausesPlayback() {
        DeviceInfo.setIsDesktopForTesting(false);

        MediaNotificationTestTabHolder tabHolder =
                new MediaNotificationTestTabHolder(1, "https://example.com", "Title");
        mHelper = tabHolder.mMediaSessionTabHelper;
        // The tabHolder creates its own mock MediaSession
        MediaSession session = tabHolder.mMediaSession;

        // Ensure session state is playing (not paused)
        mHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(true, false);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.Android.AudioBecomingNoisyPaused", true);

        // Trigger AudioBecomingNoisyReceiver
        android.content.Context context = org.robolectric.RuntimeEnvironment.getApplication();
        android.content.Intent intent =
                new android.content.Intent(android.media.AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        org.chromium.components.browser_ui.media.AudioBecomingNoisyReceiver.getInstance()
                .onReceive(context, intent);

        // Verify that the media session is suspended (SuspendType.SYSTEM = 0)
        verify(session).suspend(org.chromium.media_session.mojom.MediaSession.SuspendType.SYSTEM);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testAudioBecomingNoisy_DesktopDevice_DoesNotPause() {
        DeviceInfo.setIsDesktopForTesting(true);

        MediaNotificationTestTabHolder tabHolder =
                new MediaNotificationTestTabHolder(1, "https://example.com", "Title");
        mHelper = tabHolder.mMediaSessionTabHelper;
        MediaSession session = tabHolder.mMediaSession;

        // Ensure session state is playing (not paused)
        mHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(true, false);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.Android.AudioBecomingNoisyPaused", false);

        android.content.Context context = org.robolectric.RuntimeEnvironment.getApplication();
        android.content.Intent intent =
                new android.content.Intent(android.media.AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        org.chromium.components.browser_ui.media.AudioBecomingNoisyReceiver.getInstance()
                .onReceive(context, intent);

        // Verify that suspend is never called on desktop
        verify(session, never()).suspend(anyInt());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testAudioBecomingNoisy_TimeToResumeMetric() {
        MediaNotificationTestTabHolder tabHolder =
                new MediaNotificationTestTabHolder(1, "https://example.com", "Title");
        mHelper = tabHolder.mMediaSessionTabHelper;

        // Ensure session state is playing (not paused)
        mHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(true, false);

        // 1. Trigger the noisy event
        android.content.Context context = org.robolectric.RuntimeEnvironment.getApplication();
        android.content.Intent intent =
                new android.content.Intent(android.media.AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        org.chromium.components.browser_ui.media.AudioBecomingNoisyReceiver.getInstance()
                .onReceive(context, intent);

        // Simulate media session notifying MediaSessionHelper that it paused
        mHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(true, true);

        // Advance clock by 2.5 seconds (2500 milliseconds)
        org.robolectric.shadows.ShadowSystemClock.advanceBy(
                2500, java.util.concurrent.TimeUnit.MILLISECONDS);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.Android.AudioBecomingNoisyPaused.TimeToResume", 2500);

        // 2. Simulate playback resuming
        mHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(true, false);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testAudioBecomingNoisy_AlreadyPaused_DoesNotLogMetric() {
        MediaNotificationTestTabHolder tabHolder =
                new MediaNotificationTestTabHolder(1, "https://example.com", "Title");
        mHelper = tabHolder.mMediaSessionTabHelper;
        MediaSession session = tabHolder.mMediaSession;

        // Set session state as already paused
        mHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(true, true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Media.Android.AudioBecomingNoisyPaused")
                        .build();

        android.content.Context context = org.robolectric.RuntimeEnvironment.getApplication();
        android.content.Intent intent =
                new android.content.Intent(android.media.AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        AudioBecomingNoisyReceiver.getInstance().onReceive(context, intent);

        // Verify suspend is never called (since it's already paused)
        verify(session, never()).suspend(anyInt());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testAudioBecomingNoisyReceiver_UnregistersWhenNoObservers() {
        AudioBecomingNoisyReceiver receiver = AudioBecomingNoisyReceiver.getInstance();
        assertFalse(receiver.isRegisteredForTesting());

        AtomicInteger observer1Calls = new AtomicInteger();
        AtomicInteger observer2Calls = new AtomicInteger();
        AudioBecomingNoisyReceiver.AudioBecomingNoisyObserver observer1 =
                observer1Calls::incrementAndGet;
        AudioBecomingNoisyReceiver.AudioBecomingNoisyObserver observer2 =
                observer2Calls::incrementAndGet;

        // Adding first observer registers the receiver asynchronously; drain and verify delivery.
        AudioBecomingNoisyReceiver.addObserver(observer1);
        assertTrue(receiver.isRegisteredForTesting());
        RobolectricUtil.runAllBackgroundAndUi();

        ContextUtils.getApplicationContext()
                .sendBroadcast(new Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, observer1Calls.get());
        assertEquals(0, observer2Calls.get());

        // Adding second observer keeps it registered.
        AudioBecomingNoisyReceiver.addObserver(observer2);
        assertTrue(receiver.isRegisteredForTesting());

        // Removing one observer keeps it registered.
        AudioBecomingNoisyReceiver.removeObserver(observer1);
        assertTrue(receiver.isRegisteredForTesting());
        RobolectricUtil.runAllBackgroundAndUi();

        ContextUtils.getApplicationContext()
                .sendBroadcast(new Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, observer1Calls.get());
        assertEquals(1, observer2Calls.get());

        // Removing last observer unregisters the receiver asynchronously; drain and verify no
        // further broadcasts are delivered.
        AudioBecomingNoisyReceiver.removeObserver(observer2);
        assertFalse(receiver.isRegisteredForTesting());
        RobolectricUtil.runAllBackgroundAndUi();

        ContextUtils.getApplicationContext()
                .sendBroadcast(new Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, observer1Calls.get());
        assertEquals(1, observer2Calls.get());
    }

    @Test
    public void testAudioBecomingNoisyReceiver_RegistersAfterResetWithDroppedTask() {
        // Drop the first registration task, like the test harness does for a test that ends
        // without draining background tasks.
        PostTask.setPrenativeThreadPoolExecutorForTesting(task -> {});
        AudioBecomingNoisyReceiver.addObserver(() -> {});
        AudioBecomingNoisyReceiver.resetForTesting();
        PostTask.setPrenativeThreadPoolExecutorForTesting(RobolectricUtil.getPausedExecutor());

        AtomicInteger observerCalls = new AtomicInteger();
        AudioBecomingNoisyReceiver.addObserver(observerCalls::incrementAndGet);
        RobolectricUtil.runAllBackgroundAndUi();

        ContextUtils.getApplicationContext()
                .sendBroadcast(new Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, observerCalls.get());
    }
}
