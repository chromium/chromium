// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.media.MediaNotificationListener;

/** JUnit tests for {@link MediaNotificationController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationControllerTest extends MediaNotificationTestBase {

    @Test
    public void testOnPause_WhenAlreadyPaused_FromMediaSession_TriggersPlay() {
        setUpService();

        // Set state to PAUSED
        getController().mMediaNotificationInfo =
                mMediaNotificationInfoBuilder.setPaused(true).build();

        // Call onPause from MEDIA_SESSION (simulating hardware button press)
        getController().onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);

        // Verify onPlay is called instead of onPause (Smart Toggle)
        verify(mListener).onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
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
