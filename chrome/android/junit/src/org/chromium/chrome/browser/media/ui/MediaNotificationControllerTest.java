// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.support.v4.media.session.MediaSessionCompat;
import android.view.KeyEvent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationListener;

/** JUnit tests for {@link MediaNotificationController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationControllerTest extends MediaNotificationTestBase {

    @Test
    public void testOnMediaButtonEvent_WhenPaused_PauseDoesNotTriggerPlay() {
        MediaNotificationController controller =
                new MockMediaNotificationController(
                        new ChromeMediaNotificationControllerDelegate(
                                getNotificationId(), getMediaTypeId()));
        controller.mMediaNotificationInfo = mMediaNotificationInfoBuilder.setPaused(true).build();
        controller.mMediaSession = mock(MediaSessionCompat.class);

        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        controller.getMediaSessionCallbackForTesting().onMediaButtonEvent(intent);

        // Verify onPlay is NOT called (pause should not toggle to play).
        verify(mListener, never()).onPlay(anyInt());
    }

    @Test
    public void testOnMediaSessionCallback_OnPlayTriggersListenerPlay() {
        MediaNotificationController controller =
                new MockMediaNotificationController(
                        new ChromeMediaNotificationControllerDelegate(
                                getNotificationId(), getMediaTypeId()));
        controller.mMediaNotificationInfo = mMediaNotificationInfoBuilder.setPaused(true).build();
        controller.mMediaSession = mock(MediaSessionCompat.class);

        controller.getMediaSessionCallbackForTesting().onPlay();

        // Verify onPlay is called on the listener.
        verify(mListener).onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
    }

    @Test
    public void testOnMediaButtonEvent_WhenPaused_RecordsHistogram() {
        MediaNotificationController controller =
                new MockMediaNotificationController(
                        new ChromeMediaNotificationControllerDelegate(
                                getNotificationId(), getMediaTypeId()));
        controller.mMediaNotificationInfo = mMediaNotificationInfoBuilder.setPaused(true).build();
        controller.mMediaSession = mock(MediaSessionCompat.class);
        controller.mTimeOfLastPauseMs = 0;

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Media.Android.MediaButtonWhilePaused.TimeSincePause.Pause")
                        .build();

        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        controller.getMediaSessionCallbackForTesting().onMediaButtonEvent(intent);

        watcher.assertExpected();
    }

    @Test
    public void testOnMediaButtonEvent_WhenActionUp_DoesNotRecordHistogram() {
        MediaNotificationController controller =
                new MockMediaNotificationController(
                        new ChromeMediaNotificationControllerDelegate(
                                getNotificationId(), getMediaTypeId()));
        controller.mMediaNotificationInfo = mMediaNotificationInfoBuilder.setPaused(true).build();
        controller.mMediaSession = mock(MediaSessionCompat.class);
        controller.mTimeOfLastPauseMs = 0;

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Media.Android.MediaButtonWhilePaused.TimeSincePause.Pause")
                        .build();

        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        controller.getMediaSessionCallbackForTesting().onMediaButtonEvent(intent);

        watcher.assertExpected();
    }

    @Test
    public void testOnMediaButtonEvent_WhenPlaying_DoesNotRecordHistogram() {
        MediaNotificationController controller =
                new MockMediaNotificationController(
                        new ChromeMediaNotificationControllerDelegate(
                                getNotificationId(), getMediaTypeId()));
        controller.mMediaNotificationInfo = mMediaNotificationInfoBuilder.setPaused(false).build();
        controller.mMediaSession = mock(MediaSessionCompat.class);
        controller.mTimeOfLastPauseMs = -1;

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Media.Android.MediaButtonWhilePaused.TimeSincePause.Pause")
                        .build();

        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        controller.getMediaSessionCallbackForTesting().onMediaButtonEvent(intent);

        watcher.assertExpected();
    }

    @Test
    public void testOnPause_WhenAlreadyPaused_FromMediaSession_DoesNotTriggerPlay() {
        setUpService();

        // Set state to PAUSED
        getController().mMediaNotificationInfo =
                mMediaNotificationInfoBuilder.setPaused(true).build();

        // Call onPause from MEDIA_SESSION (simulating system-level pause command)
        getController().onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);

        // Verify onPlay is NOT called (Smart Toggle should NOT trigger from onPause)
        verify(mListener, never()).onPlay(anyInt());
        verify(mListener, never()).onPause(anyInt());
    }

    @Test
    public void testOnPause_WhenAlreadyPaused_FromOtherSource_DoesNotTriggerPlay() {
        setUpService();

        // Set state to PAUSED
        getController().mMediaNotificationInfo =
                mMediaNotificationInfoBuilder.setPaused(true).build();

        // Call onPause from HEADSET_UNPLUG (simulating unplugging headphones)
        getController().onPause(MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG);

        // Verify onPlay is NOT called (Smart Toggle should NOT trigger)
        verify(mListener, never()).onPlay(anyInt());
        // Verify onPause is NOT called (because it returns early if already paused)
        verify(mListener, never()).onPause(anyInt());
    }

    @Test
    public void testOnPause_WhenPlaying_FromMediaSession_TriggersPause() {
        setUpService();

        // Set state to PLAYING
        getController().mMediaNotificationInfo =
                mMediaNotificationInfoBuilder.setPaused(false).build();

        // Call onPause from MEDIA_SESSION
        getController().onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);

        // Verify onPause is called as expected
        verify(mListener).onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
        verify(mListener, never()).onPlay(anyInt());
    }
}
