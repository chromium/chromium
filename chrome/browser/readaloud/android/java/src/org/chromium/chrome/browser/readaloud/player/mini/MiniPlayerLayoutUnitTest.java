// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackListener;

/** Unit tests for {@link PlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerLayoutUnitTest {
    private final Activity mActivity;
    private final MiniPlayerLayout mLayout;

    @Mock
    private InteractionHandler mInteractionHandler;

    public MiniPlayerLayoutUnitTest() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mLayout = (MiniPlayerLayout) mActivity.getLayoutInflater().inflate(
                R.layout.readaloud_mini_player_layout, null);
        assertNotNull(mLayout);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testDefaultStateIsBuffering() {
        assertEquals(View.GONE, mLayout.findViewById(R.id.progress_bar).getVisibility());

        // Only the buffering layout is visible.
        assertEquals(View.VISIBLE, mLayout.findViewById(R.id.buffering_layout).getVisibility());
        assertEquals(View.GONE, mLayout.findViewById(R.id.normal_layout).getVisibility());
        assertEquals(View.GONE, mLayout.findViewById(R.id.error_layout).getVisibility());
    }

    @Test
    public void testPlayingState() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);

        assertEquals(View.VISIBLE, mLayout.findViewById(R.id.progress_bar).getVisibility());

        // Only the "normal" layout is visible.
        assertEquals(View.GONE, mLayout.findViewById(R.id.buffering_layout).getVisibility());
        assertEquals(View.VISIBLE, mLayout.findViewById(R.id.normal_layout).getVisibility());
        assertEquals(View.GONE, mLayout.findViewById(R.id.error_layout).getVisibility());

        // Can't directly test the play button drawable so instead check the a11y string.
        assertEquals("Pause", mLayout.findViewById(R.id.play_button).getContentDescription());
    }

    @Test
    public void testPausedState() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PAUSED);

        assertEquals(View.VISIBLE, mLayout.findViewById(R.id.progress_bar).getVisibility());

        // Only the "normal" layout is visible.
        assertEquals(View.GONE, mLayout.findViewById(R.id.buffering_layout).getVisibility());
        assertEquals(View.VISIBLE, mLayout.findViewById(R.id.normal_layout).getVisibility());
        assertEquals(View.GONE, mLayout.findViewById(R.id.error_layout).getVisibility());

        // Can't directly test the play button drawable so instead check the a11y string.
        assertEquals("Play", mLayout.findViewById(R.id.play_button).getContentDescription());
    }

    @Test
    public void testErrorState() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.ERROR);

        assertEquals(View.GONE, mLayout.findViewById(R.id.progress_bar).getVisibility());

        // Only the error layout is visible.
        assertEquals(View.GONE, mLayout.findViewById(R.id.buffering_layout).getVisibility());
        assertEquals(View.GONE, mLayout.findViewById(R.id.normal_layout).getVisibility());
        assertEquals(View.VISIBLE, mLayout.findViewById(R.id.error_layout).getVisibility());
    }

    @Test
    public void testSetTitle() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);
        mLayout.setTitle("Title");
        assertEquals("Title", ((TextView) mLayout.findViewById(R.id.title)).getText());
    }

    @Test
    public void testSetPublisher() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);
        mLayout.setPublisher("Publisher");
        assertEquals("Publisher", ((TextView) mLayout.findViewById(R.id.publisher)).getText());
    }

    @Test
    public void testSetProgress() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);
        mLayout.setProgress(0.5f);
        // Progress values range from 0 to 1000.
        assertEquals(500, ((ProgressBar) mLayout.findViewById(R.id.progress_bar)).getProgress());
    }

    @Test
    public void testCloseButtonClick() {
        mLayout.setInteractionHandler(mInteractionHandler);
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);

        assertTrue(mLayout.findViewById(R.id.close_button).performClick());
        verify(mInteractionHandler).onCloseClick();
    }

    @Test
    public void testExpandClick() {
        mLayout.setInteractionHandler(mInteractionHandler);
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);

        assertTrue(mLayout.findViewById(R.id.mini_player_background).performClick());
        verify(mInteractionHandler).onMiniPlayerExpandClick();
    }

    @Test
    public void testPlayButtonClick() {
        mLayout.setInteractionHandler(mInteractionHandler);
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);

        assertTrue(mLayout.findViewById(R.id.play_button).performClick());
        verify(mInteractionHandler).onPlayPauseClick();
    }
}
