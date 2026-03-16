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

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationListener;

/** JUnit tests for {@link MediaNotificationController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationControllerTest extends MediaNotificationTestBase {

    @Test
    public void testOnMediaButton_WhenAlreadyPaused_SyntheticPauseTriggersPlay() {
        // Arrange: paused state
        getController().mMediaNotificationInfo =
                mMediaNotificationInfoBuilder.setPaused(true).build();

        // Create a media button intent with KEYCODE_MEDIA_PAUSE and eventTime = 0 (synthetic)
        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        // Call maybeTogglePausedPlayback. It should return true because it's synthetic.
        Assert.assertTrue(getController().maybeTogglePausedPlayback(intent));

        // Verify onPlay is called
        verify(mListener).onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
    }

    @Test
    public void testOnMediaButton_WhenAlreadyPaused_SystemPauseDoesNotTriggerPlay() {
        // Arrange: paused state
        getController().mMediaNotificationInfo =
                mMediaNotificationInfoBuilder.setPaused(true).build();

        // Create a media button intent with KEYCODE_MEDIA_PAUSE and eventTime > 0 (system)
        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event =
                new KeyEvent(100, 100, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        // Call maybeTogglePausedPlayback. It should return false because it's a system event.
        Assert.assertFalse(getController().maybeTogglePausedPlayback(intent));

        // Verify onPlay is NOT called
        verify(mListener, never()).onPlay(anyInt());
    }

    @Test
    public void testOnMediaButtonEvent_DelegatesToMaybeTogglePausedPlayback() {
        // Create a fresh controller to avoid spy vs inner class state discrepancy
        MediaNotificationController controller =
                new MockMediaNotificationController(
                        new ChromeMediaNotificationControllerDelegate(getNotificationId()));
        controller.mMediaNotificationInfo = mMediaNotificationInfoBuilder.setPaused(true).build();

        // Mock MediaSession for the controller
        controller.mMediaSession = mock(MediaSessionCompat.class);

        // Create a media button intent with KEYCODE_MEDIA_PAUSE and eventTime = 0 (synthetic)
        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE, 0);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, event);

        // Call onMediaButtonEvent on the callback retrieved via the new getter
        boolean handled = controller.getMediaSessionCallbackForTesting().onMediaButtonEvent(intent);

        // Verify it was handled and onPlay was called on the listener
        Assert.assertTrue(handled);
        verify(mListener).onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
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
