// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Tests for {@link ImmersiveVideoPoseManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoPoseManagerTest {
    private static final float DELTA = 1e-5f;
    private static final float QUAD_LAYOUT_HEIGHT = 0.5f;
    private static final float SPHERE_LAYOUT_HEIGHT = 0.6f;

    @Mock private ImmersiveVideoPoseManager.Delegate mDelegate;

    private ImmersiveVideoPoseManager mManager;

    @Before
    public void setUp() {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        when(mDelegate.getLayoutHeight()).thenReturn(QUAD_LAYOUT_HEIGHT);
        mManager = new ImmersiveVideoPoseManager(mDelegate);
    }

    @Test
    public void testDefaultPoses() {
        XrPose playerPose = mManager.getPlayerPanelPose(ImmersiveProjectionType.QUAD);
        assertEquals(0f, playerPose.getTranslation().getX(), DELTA);
        assertEquals(0f, playerPose.getTranslation().getY(), DELTA);
        assertEquals(0.5f, playerPose.getTranslation().getZ(), DELTA);

        assertEquals(0f, playerPose.getRotation().getX(), DELTA);
        assertEquals(0f, playerPose.getRotation().getY(), DELTA);
        assertEquals(0f, playerPose.getRotation().getZ(), DELTA);
        assertEquals(1f, playerPose.getRotation().getW(), DELTA);
    }

    @Test
    public void testQuadMode_PoseUpdates() {
        XrPose newPose =
                XrPose.create(XrVector3.create(1f, 2f, 3f), XrQuaternion.create(0f, 1f, 0f, 0f));

        mManager.onPlayerPanelPoseChanged(newPose, ImmersiveProjectionType.QUAD);

        XrPose actualPlayerPose = mManager.getPlayerPanelPose(ImmersiveProjectionType.QUAD);
        assertEquals(1f, actualPlayerPose.getTranslation().getX(), DELTA);
        assertEquals(2f, actualPlayerPose.getTranslation().getY(), DELTA);
        assertEquals(3f, actualPlayerPose.getTranslation().getZ(), DELTA);
        assertEquals(1f, actualPlayerPose.getRotation().getY(), DELTA);

        // Control panel pose in QUAD mode should only be vertical offset
        XrPose actualControlPose = mManager.getControlPanelPose(ImmersiveProjectionType.QUAD);
        assertEquals(0f, actualControlPose.getTranslation().getX(), DELTA);
        assertEquals(-0.25f, actualControlPose.getTranslation().getY(), DELTA);
        assertEquals(0f, actualControlPose.getTranslation().getZ(), DELTA);
        assertEquals(1f, actualControlPose.getRotation().getW(), DELTA);
    }

    @Test
    public void testSphereMode_PoseUpdates() {
        // In SPHERE mode, player panel pose should be pinned to origin and identity
        XrPose actualPlayerPose = mManager.getPlayerPanelPose(ImmersiveProjectionType.SPHERE);
        assertEquals(0f, actualPlayerPose.getTranslation().getX(), DELTA);
        assertEquals(0f, actualPlayerPose.getTranslation().getY(), DELTA);
        assertEquals(0f, actualPlayerPose.getTranslation().getZ(), DELTA);
        assertEquals(1f, actualPlayerPose.getRotation().getW(), DELTA);

        when(mDelegate.getLayoutHeight()).thenReturn(SPHERE_LAYOUT_HEIGHT);

        // Move control panel in SPHERE mode
        XrPose controlPose =
                XrPose.create(XrVector3.create(5f, 6f, 7f), XrQuaternion.create(0f, 0f, 1f, 0f));

        mManager.onControlPanelPoseChanged(controlPose, ImmersiveProjectionType.SPHERE);

        // Verify control panel pose query returns the expected values
        XrPose actualControlPose = mManager.getControlPanelPose(ImmersiveProjectionType.SPHERE);
        assertEquals(5f, actualControlPose.getTranslation().getX(), DELTA);
        assertEquals(6f, actualControlPose.getTranslation().getY(), DELTA);
        assertEquals(7f, actualControlPose.getTranslation().getZ(), DELTA);
        assertEquals(1f, actualControlPose.getRotation().getZ(), DELTA);

        // Verify that when switching back to QUAD mode, the player panel inherited the center pose
        XrPose actualPlayerPoseQuad = mManager.getPlayerPanelPose(ImmersiveProjectionType.QUAD);
        assertEquals(5f, actualPlayerPoseQuad.getTranslation().getX(), DELTA);
        assertEquals(6.3f, actualPlayerPoseQuad.getTranslation().getY(), DELTA);
        assertEquals(7f, actualPlayerPoseQuad.getTranslation().getZ(), DELTA);
        assertEquals(1f, actualPlayerPoseQuad.getRotation().getZ(), DELTA);
    }
}
