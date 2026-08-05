// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.chromium.build.NullUtil.assumeNonNull;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.os.Handler;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ImmersiveVideoControlMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoControlMediatorTest {
    @Mock private ImmersiveVideoControlCoordinator.Delegate mDelegate;

    private PropertyModel mModel;
    private ImmersiveVideoControlMediator mMediator;
    private int mModelUpdateCount;
    private int mProgressUpdateCount;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mModel =
                new PropertyModel.Builder(ImmersiveVideoControlProperties.ALL_KEYS)
                        .with(ImmersiveVideoControlProperties.IS_PLAYING, false)
                        .build();
        mModel.addObserver(
                (source, propertyKey) -> {
                    mModelUpdateCount++;
                    if (propertyKey == ImmersiveVideoControlProperties.PROGRESS) {
                        mProgressUpdateCount++;
                    }
                });
        mMediator = new ImmersiveVideoControlMediator(mModel, mDelegate);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testHiddenStopsUpdatesAndReshowResynchronizesSingleLoop() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);

        assertTrue(mMediator.hasPendingSeekbarUpdateForTesting());
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        assertEquals(1_200, getProgress());

        mMediator.setVisible(false);
        int hiddenProgress = getProgress();
        int hiddenModelUpdateCount = mModelUpdateCount;
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());

        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 10_000, /* playbackRate= */ 2.0);
        ShadowLooper.idleMainLooper(1_000, TimeUnit.MILLISECONDS);
        assertEquals(hiddenProgress, getProgress());
        assertEquals(hiddenModelUpdateCount, mModelUpdateCount);

        mMediator.setVisible(true);
        assertEquals(12_000, getProgress());
        assertTrue(mMediator.hasPendingSeekbarUpdateForTesting());

        mMediator.setVisible(true);
        int progressUpdatesBeforeTicking = mProgressUpdateCount;
        ShadowLooper.idleMainLooper(150, TimeUnit.MILLISECONDS);
        assertEquals(12_300, getProgress());
        assertEquals(progressUpdatesBeforeTicking + 3, mProgressUpdateCount);
    }

    @Test
    public void testReshowSchedulesExactlyOneSeekbarLoop() {
        mMediator.destroy();
        Handler handler = mock(Handler.class);
        mMediator = new ImmersiveVideoControlMediator(mModel, mDelegate, handler);
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        mMediator.setVisible(false);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 10_000, /* playbackRate= */ 1.0);
        clearInvocations(handler);

        mMediator.setVisible(true);
        mMediator.setVisible(true);

        ArgumentCaptor<Runnable> taskCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(handler, times(1)).post(taskCaptor.capture());
        Runnable seekbarTask = taskCaptor.getValue();

        clearInvocations(handler);
        seekbarTask.run();

        verify(handler, times(1)).postDelayed(same(seekbarTask), eq(50L));
    }

    @Test
    public void testPauseAndSeekStopAndRestartTimerAsIntended() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        assertEquals(1_100, getProgress());

        mMediator.onStartTrackingTouch();
        int progressAtSeekStart = getProgress();
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        assertEquals(progressAtSeekStart, getProgress());

        mMediator.onSeekTo(5_000);
        verify(mDelegate).seekTo(5_000);
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 2_000, /* playbackRate= */ 1.0);
        mMediator.onStopTrackingTouch();
        assertTrue(mMediator.hasPendingSeekbarUpdateForTesting());
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        assertEquals(5_300, getProgress());

        mMediator.updatePlaybackState(false);
        int pausedProgress = getProgress();
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        assertEquals(pausedProgress, getProgress());

        mMediator.updatePlaybackState(true);
        assertTrue(mMediator.hasPendingSeekbarUpdateForTesting());
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000,
                /* positionMs= */ pausedProgress,
                /* playbackRate= */ 0.0);
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());
    }

    @Test
    public void testStationarySeekPreservesElapsedPlaybackAtSeekStart() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        mMediator.onStartTrackingTouch();
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        mMediator.onStopTrackingTouch();
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        assertEquals(1_700, getProgress());
    }

    @Test
    public void testDurationShrinkDuringSeekDefersRangeAndClampsOnStop() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 30_000, /* playbackRate= */ 1.0);

        mMediator.onStartTrackingTouch();
        mMediator.onSeekTo(50_000);
        mMediator.updateMediaPosition(
                /* durationMs= */ 40_000, /* positionMs= */ 30_000, /* playbackRate= */ 1.0);

        assertEquals(60_000, getMaxProgress());
        assertEquals(50_000, getProgress());

        mMediator.onSeekTo(55_000);
        verify(mDelegate).seekTo(40_000);
        mMediator.onStopTrackingTouch();

        assertEquals(40_000, getMaxProgress());
        assertEquals(40_000, getProgress());
    }

    @Test
    public void testRateChangeDuringSeekUsesOldRateForElapsedSegment() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        mMediator.onStartTrackingTouch();
        mMediator.onSeekTo(5_000);
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);

        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 2_000, /* playbackRate= */ 2.0);
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        mMediator.onStopTrackingTouch();

        assertEquals(6_500, getProgress());
    }

    @Test
    public void testPlaybackStateChangeDuringSeekDoesNotMoveThumb() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        mMediator.onStartTrackingTouch();
        mMediator.onSeekTo(5_000);
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);

        mMediator.updatePlaybackState(false);

        assertEquals(5_000, getProgress());
        mMediator.onStopTrackingTouch();
        assertEquals(5_500, getProgress());
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());
    }

    @Test
    public void testHideCancelsInterruptedSeek() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        mMediator.onStartTrackingTouch();
        mMediator.onSeekTo(5_000);

        mMediator.setVisible(false);
        ShadowLooper.idleMainLooper(1_000, TimeUnit.MILLISECONDS);
        mMediator.setVisible(true);

        assertEquals(6_000, getProgress());
        assertTrue(mMediator.hasPendingSeekbarUpdateForTesting());
    }

    @Test
    public void testDestroyCancelsCallbacksAndMakesAllInputsNoOps() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 60_000, /* positionMs= */ 1_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);
        assertTrue(mMediator.hasPendingSeekbarUpdateForTesting());

        mMediator.destroy();
        mMediator.destroy();
        int updateCountAfterDestroy = mModelUpdateCount;
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());

        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 30_000, /* positionMs= */ 5_000, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(false);
        mMediator.setFormatButtonSelected(true);
        mMediator.setMovable(true);
        mMediator.onPlayClicked();
        mMediator.onPauseClicked();
        mMediator.onFormatClicked();
        mMediator.onExitFullscreenClicked();
        mMediator.onStartTrackingTouch();
        mMediator.onSeekTo(10_000);
        mMediator.onStopTrackingTouch();
        ShadowLooper.idleMainLooper(1_000, TimeUnit.MILLISECONDS);

        assertEquals(updateCountAfterDestroy, mModelUpdateCount);
        assertFalse(mMediator.hasPendingSeekbarUpdateForTesting());
        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testDisplayedPositionAndLabelClampAtDuration() {
        mMediator.setVisible(true);
        mMediator.updateMediaPosition(
                /* durationMs= */ 1_000, /* positionMs= */ 900, /* playbackRate= */ 1.0);
        mMediator.updatePlaybackState(true);

        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);

        assertEquals(1_000, getProgress());
        assertEquals("00:01", mModel.get(ImmersiveVideoControlProperties.POSITION_TEXT));
    }

    private int getProgress() {
        return assumeNonNull(mModel.get(ImmersiveVideoControlProperties.PROGRESS));
    }

    private int getMaxProgress() {
        return assumeNonNull(mModel.get(ImmersiveVideoControlProperties.MAX_PROGRESS));
    }
}
