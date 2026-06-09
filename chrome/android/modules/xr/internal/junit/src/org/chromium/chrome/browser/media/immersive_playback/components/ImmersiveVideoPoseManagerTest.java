// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.junit.Assert.assertArrayEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.ImmersiveProjectionType;

/** Tests for {@link ImmersiveVideoPoseManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoPoseManagerTest {
    private static final float[] IDENTITY_ROTATION = new float[] {0f, 0f, 0f, 1f};
    private static final float[] ORIGIN_TRANSLATION = new float[] {0f, 0f, 0f};
    private static final float DELTA = 1e-5f;
    private static final float QUAD_LAYOUT_HEIGHT = 0.5f;
    private static final float SPHERE_LAYOUT_HEIGHT = 0.6f;

    @Mock private ImmersiveVideoPoseManager.Delegate mDelegate;

    private ImmersiveVideoPoseManager mManager;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        when(mDelegate.getLayoutHeight()).thenReturn(QUAD_LAYOUT_HEIGHT);
        mManager = new ImmersiveVideoPoseManager(mDelegate);
    }

    @Test
    public void testDefaultPoses() {
        assertArrayEquals(
                new float[] {0f, 0f, 0.5f},
                mManager.getPlayerPanelTranslation(ImmersiveProjectionType.QUAD),
                DELTA);
        assertArrayEquals(
                IDENTITY_ROTATION,
                mManager.getPlayerPanelRotation(ImmersiveProjectionType.QUAD),
                DELTA);
    }

    @Test
    public void testQuadMode_PoseUpdates() {
        float[] newTrans = new float[] {1f, 2f, 3f};
        float[] newRot = new float[] {0f, 1f, 0f, 0f};

        mManager.onPlayerPanelPoseChanged(newTrans, newRot, ImmersiveProjectionType.QUAD);

        assertArrayEquals(
                newTrans, mManager.getPlayerPanelTranslation(ImmersiveProjectionType.QUAD), DELTA);
        assertArrayEquals(
                newRot, mManager.getPlayerPanelRotation(ImmersiveProjectionType.QUAD), DELTA);

        // Control panel translation in QUAD mode should only be vertical offset
        assertArrayEquals(
                new float[] {0f, -0.25f, 0f},
                mManager.getControlPanelTranslation(ImmersiveProjectionType.QUAD),
                DELTA);
        assertArrayEquals(
                IDENTITY_ROTATION,
                mManager.getControlPanelRotation(ImmersiveProjectionType.QUAD),
                DELTA);
    }

    @Test
    public void testSphereMode_PoseUpdates() {
        // In SPHERE mode, player panel pose should be pinned to origin and identity
        assertArrayEquals(
                ORIGIN_TRANSLATION,
                mManager.getPlayerPanelTranslation(ImmersiveProjectionType.SPHERE),
                DELTA);
        assertArrayEquals(
                IDENTITY_ROTATION,
                mManager.getPlayerPanelRotation(ImmersiveProjectionType.SPHERE),
                DELTA);

        when(mDelegate.getLayoutHeight()).thenReturn(SPHERE_LAYOUT_HEIGHT);

        // Move control panel in SPHERE mode
        float[] controlTrans = new float[] {5f, 6f, 7f};
        float[] controlRot = new float[] {0f, 0f, 1f, 0f};

        mManager.onControlPanelPoseChanged(
                controlTrans, controlRot, ImmersiveProjectionType.SPHERE);

        // Verify control panel pose query returns the expected values
        assertArrayEquals(
                controlTrans,
                mManager.getControlPanelTranslation(ImmersiveProjectionType.SPHERE),
                DELTA);
        assertArrayEquals(
                controlRot,
                mManager.getControlPanelRotation(ImmersiveProjectionType.SPHERE),
                DELTA);

        // Verify that when switching back to QUAD mode, the player panel inherited the center pose
        assertArrayEquals(
                new float[] {5f, 6.3f, 7f},
                mManager.getPlayerPanelTranslation(ImmersiveProjectionType.QUAD),
                DELTA);
        assertArrayEquals(
                controlRot, mManager.getPlayerPanelRotation(ImmersiveProjectionType.QUAD), DELTA);
    }
}
