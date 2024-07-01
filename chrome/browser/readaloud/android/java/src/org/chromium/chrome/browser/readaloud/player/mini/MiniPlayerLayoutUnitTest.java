// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Unit tests for {@link PlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class MiniPlayerLayoutUnitTest {
    private final Activity mActivity;
    private MiniPlayerLayout mLayout;

    @Mock private InteractionHandler mInteractionHandler;
    @Mock private MiniPlayerMediator mMediator;

    public MiniPlayerLayoutUnitTest() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLayout =
                (MiniPlayerLayout)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.readaloud_mini_player_layout, null);
        assertNotNull(mLayout);
        mLayout.setMediator(mMediator);
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
    public void testStoppedState() {
        mLayout.onPlaybackStateChanged(PlaybackListener.State.STOPPED);

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
    public void testFadeInWithoutAnimation() {
        assertEquals(0f, mLayout.getAlpha(), /* delta= */ 0f);

        mLayout.enableAnimations(false);
        mLayout.changeOpacity(0f, 1f);

        assertNull(mLayout.getAnimatorForTesting());
        assertEquals(1f, mLayout.getAlpha(), /* delta= */ 0f);
        verify(mMediator).onFullOpacityReached(any(View.class));
    }

    @Test
    public void testFadeInWithAnimation() {
        assertEquals(0f, mLayout.getAlpha(), /* delta= */ 0f);

        mLayout.enableAnimations(true);
        mLayout.changeOpacity(0f, 1f);

        ObjectAnimator animator = mLayout.getAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(300L, animator.getDuration());

        animator.end();
        assertEquals(1f, mLayout.getAlpha(), /* delta= */ 0f);
        verify(mMediator).onFullOpacityReached(any(View.class));
    }

    @Test
    public void testFadeOutWithoutAnimation() {
        // Fade in first.
        mLayout.enableAnimations(false);
        mLayout.changeOpacity(0f, 1f);

        // Ensure we're starting with full opacity.
        assertEquals(1f, mLayout.getAlpha(), /* delta= */ 0f);

        // Fade out.
        mLayout.changeOpacity(1f, 0f);

        assertNull(mLayout.getAnimatorForTesting());
        assertEquals(0f, mLayout.getAlpha(), /* delta= */ 0f);
        verify(mMediator).onZeroOpacityReached();
    }

    @Test
    public void testFadeOutWithAnimation() {
        // Fade in first.
        mLayout.enableAnimations(false);
        mLayout.changeOpacity(0f, 1f);

        // Ensure we're starting with full opacity.
        assertEquals(1f, mLayout.getAlpha(), /* delta= */ 0f);

        // Fade out.
        mLayout.enableAnimations(true);
        mLayout.changeOpacity(1f, 0f);

        ObjectAnimator animator = mLayout.getAnimatorForTesting();
        assertNotNull(animator);
        assertEquals(300L, animator.getDuration());

        animator.end();
        assertEquals(0f, mLayout.getAlpha(), /* delta= */ 0f);
        verify(mMediator).onZeroOpacityReached();
    }

    @Test
    public void testAlreadyFadedInAndOut() {
        mLayout.enableAnimations(false);

        // 0 -> 1
        mLayout.changeOpacity(0f, 1f);
        verify(mMediator).onFullOpacityReached(any(View.class));
        reset(mMediator);

        // 0 -> 1 again has no effect.
        mLayout.changeOpacity(0f, 1f);
        verify(mMediator, never()).onFullOpacityReached(any(View.class));

        // 1 -> 0
        mLayout.changeOpacity(1f, 0f);
        verify(mMediator).onZeroOpacityReached();
        reset(mMediator);

        // 1 -> 0 again has no effect.
        mLayout.changeOpacity(1f, 0f);
        verify(mMediator, never()).onZeroOpacityReached();
    }

    @Test
    public void testDestroyCancelsAnimation() {
        mLayout.enableAnimations(true);
        mLayout.changeOpacity(0f, 1f);
        assertNotNull(mLayout.getAnimatorForTesting());

        mLayout.destroy();
        assertNull(mLayout.getAnimatorForTesting());
    }

    @Test
    public void testOnLayoutZeroHeight() {
        mLayout.onLayout(true, 0, 0, 0, 0);
        verify(mMediator, never()).onHeightKnown(anyInt());
    }

    @Test
    public void testOnLayoutGetsHeight() {
        // Fake the backdrop height so onLayout() doesn't return early.
        View spyBackdrop = replaceWithSpy(R.id.backdrop);
        mLayout.onFinishInflate();
        doReturn(187).when(spyBackdrop).getHeight();
        assertEquals(187, mLayout.findViewById(R.id.backdrop).getHeight());

        mLayout.onLayout(true, 0, 0, 0, 0);

        verify(mMediator).onHeightKnown(eq(187));
    }

    @Test
    public void testOnLayoutSetsCloseButtonTouchDelegate() {
        // Fake the backdrop height so onLayout() doesn't return early.
        View spyBackdrop = replaceWithSpy(R.id.backdrop);
        mLayout.onFinishInflate();
        doReturn(187).when(spyBackdrop).getHeight();
        assertEquals(187, mLayout.findViewById(R.id.backdrop).getHeight());

        mLayout.onLayout(true, 0, 0, 0, 0);

        TouchDelegate delegate =
                ((View) mLayout.findViewById(R.id.close_button).getParent()).getTouchDelegate();
        assertNotNull(delegate);
    }

    @Test
    @Config(qualifiers = "night")
    public void testDarkModeBackgroundColor() {
        View spyBackdrop = replaceWithSpy(R.id.backdrop);
        mLayout.onFinishInflate();
        int bg = ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_4);
        verify(spyBackdrop).setBackgroundColor(eq(bg));
        verify(mMediator).onBackgroundColorUpdated(eq(bg));
    }

    private View replaceWithSpy(int childId) {
        View original = mLayout.findViewById(childId);
        ViewGroup parent = (ViewGroup) original.getParent();

        int index = parent.indexOfChild(original);
        parent.removeViewAt(index);

        View spy = Mockito.spy(original);
        parent.addView(spy, index);
        assertEquals(spy, parent.findViewById(childId));
        return spy;
    }
}
