// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ImmersiveVideoControlAutoHideManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoControlAutoHideManagerTest {
    private static final long TEST_DELAY_MS = 1000L;

    @Mock private Runnable mMockRunnable;
    private ImmersiveVideoControlAutoHideManager mManager;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mManager = new ImmersiveVideoControlAutoHideManager(mMockRunnable, TEST_DELAY_MS);
    }

    @Test
    public void testStartTimer_FiresAfterDelay() {
        mManager.startTimer();

        ShadowLooper.idleMainLooper(TEST_DELAY_MS - 100, TimeUnit.MILLISECONDS);
        verify(mMockRunnable, never()).run();

        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        verify(mMockRunnable).run();
    }

    @Test
    public void testStopTimer_CancelsExecution() {
        mManager.startTimer();
        mManager.stopTimer();

        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable, never()).run();
    }

    @Test
    public void testHover_PreventsTimerFromFiring() {
        mManager.startTimer();
        mManager.onControlPanelHoverChanged(true);

        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable, never()).run();

        // Exiting hover should restart the timer
        mManager.onControlPanelHoverChanged(false);
        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable).run();
    }

    @Test
    public void testMovement_PreventsTimerFromFiring() {
        mManager.startTimer();
        mManager.onControlPanelMoveChanged(true);

        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable, never()).run();

        // Ending movement should restart the timer
        mManager.onControlPanelMoveChanged(false);
        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable).run();
    }

    @Test
    public void testFormatPanelHover_PreventsTimerFromFiring() {
        mManager.startTimer();
        mManager.onFormatPanelHoverChanged(true);

        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable, never()).run();

        mManager.onFormatPanelHoverChanged(false);
        ShadowLooper.idleMainLooper(TEST_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mMockRunnable).run();
    }
}
