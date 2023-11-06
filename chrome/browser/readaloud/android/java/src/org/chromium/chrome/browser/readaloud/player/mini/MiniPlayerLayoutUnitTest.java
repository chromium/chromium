// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.os.SystemClock;
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
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.PlaybackListener;

/** Unit tests for {@link PlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class MiniPlayerLayoutUnitTest {
    private static final float HEIGHT = 210f;
    private static final long ANIMATION_DURATION_MS = 150L;
    private static final int INITIAL_SYSTEM_TIME_MS = 1000;
    private final Activity mActivity;
    private final MiniPlayerLayout mLayout;

    @Mock private InteractionHandler mInteractionHandler;
    @Mock private MiniPlayerMediator mMediator;

    public MiniPlayerLayoutUnitTest() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mLayout =
                (MiniPlayerLayout)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.readaloud_mini_player_layout, null);
        assertNotNull(mLayout);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLayout.setMediator(mMediator);
        SystemClock.setCurrentTimeMillis(INITIAL_SYSTEM_TIME_MS);
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

        assertTrue(mLayout.findViewById(R.id.mini_player_container).performClick());
        verify(mInteractionHandler).onMiniPlayerExpandClick();
    }

    @Test
    public void testPlayButtonClick() {
        mLayout.setInteractionHandler(mInteractionHandler);
        mLayout.onPlaybackStateChanged(PlaybackListener.State.PLAYING);

        assertTrue(mLayout.findViewById(R.id.play_button).performClick());
        verify(mInteractionHandler).onPlayPauseClick();
    }

    @Test
    public void testShow() {
        mLayout.enableAnimations(false);
        mLayout.updateVisibility(VisibilityState.SHOWING);

        // No animation, so state should immediately become VISIBLE.
        assertEquals(View.VISIBLE, mLayout.getVisibility());
        assertEquals(0f, mLayout.getTranslationY(), /* delta= */ 1e-6);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.VISIBLE));
    }

    @Test
    public void testShow_alreadyShowing() {
        // Show once.
        mLayout.enableAnimations(false);
        mLayout.updateVisibility(VisibilityState.SHOWING);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.VISIBLE));

        // Showing again shouldn't have an effect.
        reset(mMediator);
        mLayout.updateVisibility(VisibilityState.SHOWING);
        verify(mMediator, never()).onVisibilityChanged(anyInt());
    }

    @Test
    public void testHide() {
        mLayout.enableAnimations(false);

        // Show.
        mLayout.updateVisibility(VisibilityState.SHOWING);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.VISIBLE));

        // Hide.
        mLayout.updateVisibility(VisibilityState.HIDING);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.GONE));
        assertEquals(View.GONE, mLayout.getVisibility());
        assertEquals(0f, mLayout.getTranslationY(), /* delta= */ 1e-6);
    }

    @Test
    public void testHide_alreadyHiding() {
        mLayout.enableAnimations(false);
        assertEquals(View.GONE, mLayout.getVisibility());
        // No effect yet since initial state is GONE.
        mLayout.updateVisibility(VisibilityState.HIDING);
        verify(mMediator, never()).onVisibilityChanged(anyInt());
    }

    @Test
    public void testAnimatedShow() {
        // Start show animation.
        mLayout.enableAnimations(true);
        mLayout.updateVisibility(VisibilityState.SHOWING);

        // Visibility gets set from GONE to INVISIBLE to trigger layout.
        assertEquals(View.INVISIBLE, mLayout.getVisibility());

        // Pretend the layout step was triggered so the real height will be calculated.
        mLayout.layout(0, 0, 0, (int) HEIGHT);
        assertEquals((int) HEIGHT, mLayout.getHeight());

        // Set up and run the animation.
        // Start out visible and translated down by HEIGHT pixels.
        assertEquals(View.VISIBLE, mLayout.getVisibility());
        assertEquals(HEIGHT, mLayout.getTranslationY(), /* delta= */ 1e-6);

        ObjectAnimator animator = mLayout.getYTranslationAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(HEIGHT, (Float) animator.getAnimatedValue(), /* delta= */ 1e-6);
        assertEquals(150L, animator.getDuration());

        // Skip to the end.
        animator.end();

        // Translation is now 0, so the view is in its fully visible position.
        assertEquals(0f, (Float) animator.getAnimatedValue(), /* delta= */ 1e-6);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.VISIBLE));
    }

    @Test
    public void testAnimatedHide() {
        mLayout.layout(0, 0, 0, (int) HEIGHT);
        assertEquals((int) HEIGHT, mLayout.getHeight());

        mLayout.enableAnimations(false);
        mLayout.updateVisibility(VisibilityState.VISIBLE);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.VISIBLE));
        reset(mMediator);

        // Start hide animation.
        mLayout.enableAnimations(true);
        mLayout.updateVisibility(VisibilityState.HIDING);

        // Start out visible and with no translation.
        assertEquals(View.VISIBLE, mLayout.getVisibility());
        assertEquals(0f, mLayout.getTranslationY(), /* delta= */ 1e-6);

        ObjectAnimator animator = mLayout.getYTranslationAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(0f, (Float) animator.getAnimatedValue(), /* delta= */ 1e-6);
        assertEquals(150L, animator.getDuration());

        // Skip to the end.
        animator.end();

        // Translation is now 0, so the view is in its fully visible position.
        assertEquals(HEIGHT, (Float) animator.getAnimatedValue(), /* delta= */ 1e-6);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.GONE));
    }

    @Test
    public void testAnimatedShowTwice() {
        // Start show animation.
        mLayout.enableAnimations(true);
        mLayout.updateVisibility(VisibilityState.SHOWING);

        // Pretend layout to set view height.
        mLayout.layout(0, 0, 0, (int) HEIGHT);
        assertEquals((int) HEIGHT, mLayout.getHeight());

        // Animation should be started and not yet ended.
        ObjectAnimator animator = mLayout.getYTranslationAnimatorForTesting();
        assertTrue(animator.isStarted());

        // Calling updateVisibility(SHOWING) again shouldn't interrupt the ongoing animation.
        mLayout.updateVisibility(VisibilityState.SHOWING);
        assertEquals(animator, mLayout.getYTranslationAnimatorForTesting());
        assertTrue(animator.isStarted());
    }

    @Test
    public void testAnimatedHideDuringShow() {
        // Start show animation.
        mLayout.enableAnimations(true);
        mLayout.updateVisibility(VisibilityState.SHOWING);

        // Pretend layout to set view height.
        mLayout.layout(0, 0, 0, (int) HEIGHT);
        assertEquals((int) HEIGHT, mLayout.getHeight());

        // Animation should be started and not yet ended.
        ObjectAnimator animator = mLayout.getYTranslationAnimatorForTesting();
        assertTrue(animator.isStarted());

        // Pretend some time has passed.
        long elapsedMs = 100L;
        animator.setCurrentPlayTime(elapsedMs);

        // Start hide animation while the show animation is still running.
        mLayout.updateVisibility(VisibilityState.HIDING);

        // There should be a new animator for hiding.
        ObjectAnimator newAnimator = mLayout.getYTranslationAnimatorForTesting();
        assertNotNull(newAnimator);
        assertNotEquals(animator, newAnimator);
        assertTrue(newAnimator.isStarted());

        // The new animation should have started at (duration - 100) ms.
        assertEquals((ANIMATION_DURATION_MS - elapsedMs), newAnimator.getCurrentPlayTime());
    }

    @Test
    public void testDestroyCancelsAnimation() {
        mLayout.layout(0, 0, 0, (int) HEIGHT);
        assertEquals((int) HEIGHT, mLayout.getHeight());
        mLayout.enableAnimations(true);
        mLayout.updateVisibility(VisibilityState.SHOWING);

        ObjectAnimator animator = mLayout.getYTranslationAnimatorForTesting();
        assertTrue(animator.isStarted());

        mLayout.destroy();
        assertFalse(animator.isStarted());
    }

    @Test
    public void testAnimatedShowHideShow() {
        /// Show #1

        // Start show animation.
        mLayout.enableAnimations(true);
        mLayout.updateVisibility(VisibilityState.SHOWING);

        // Pretend the layout step was triggered so the real height will be calculated.
        mLayout.layout(0, 0, 0, (int) HEIGHT);
        assertEquals((int) HEIGHT, mLayout.getHeight());

        assertEquals(HEIGHT, mLayout.getTranslationY(), /* delta= */ 1e-6);

        ObjectAnimator animator = mLayout.getYTranslationAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(HEIGHT, (Float) animator.getAnimatedValue(), /* delta= */ 1e-6);
        assertEquals(150L, animator.getDuration());

        // Skip to the end.
        animator.end();

        // Translation is now 0, so the view is in its fully visible position.
        assertEquals(0f, mLayout.getTranslationY(), /* delta= */ 1e-6);
        verify(mMediator).onVisibilityChanged(eq(VisibilityState.VISIBLE));

        /// Hide

        mLayout.updateVisibility(VisibilityState.HIDING);
        assertEquals(View.VISIBLE, mLayout.getVisibility());
        assertEquals(0f, mLayout.getTranslationY(), /* delta= */ 1e-6);

        animator = mLayout.getYTranslationAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(0f, (Float) animator.getAnimatedValue(), /* delta= */ 1e-6);
        assertEquals(150L, animator.getDuration());

        animator.end();

        assertEquals(HEIGHT, mLayout.getTranslationY(), /* delta= */ 1e-6);

        /// Show #2

        mLayout.updateVisibility(VisibilityState.SHOWING);

        assertEquals(HEIGHT, mLayout.getTranslationY(), /* delta= */ 1e-6);

        mLayout.getYTranslationAnimatorForTesting().end();

        assertEquals(0f, mLayout.getTranslationY(), /* delta= */ 1e-6);
    }
}
