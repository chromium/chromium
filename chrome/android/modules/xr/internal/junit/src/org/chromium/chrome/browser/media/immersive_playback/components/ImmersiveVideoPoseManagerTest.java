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
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Tests for {@link ImmersiveVideoPoseManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoPoseManagerTest {
    static {
        XrModuleProviderImpl.initialize();
    }

    private static final float QUAD_LAYOUT_HEIGHT = 0.5f;
    private static final float SPHERE_LAYOUT_HEIGHT = 0.6f;
    private static final float DEFAULT_CURVE_RADIUS = 5f;
    private static final float SPHERE_CURVE_RADIUS = 4f;
    private static final float EPSILON = 1e-4f;

    // Default Z distance of the player panel in QUAD mode.
    private static final float DEFAULT_PLAYER_Z = 0.5f;
    // Forward Z offset of the control panel relative to the player panel.
    private static final float CONTROL_OFFSET_Z = 0.04f;
    // In SPHERE/HEMISPHERE mode, the default control panel Z in unrotated world space is
    // DEFAULT_PLAYER_Z + CONTROL_OFFSET_Z (0.54m).
    private static final float WORLD_CONTROL_OFFSET_Z = DEFAULT_PLAYER_Z + CONTROL_OFFSET_Z;

    @Mock private ImmersiveVideoPoseManager.Delegate mDelegate;

    private ImmersiveVideoPoseManager mManager;

    @Before
    public void setUp() {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        when(mDelegate.getLayoutHeight()).thenReturn(QUAD_LAYOUT_HEIGHT);
        when(mDelegate.getCurveRadius()).thenReturn(DEFAULT_CURVE_RADIUS);
        mManager = new ImmersiveVideoPoseManager(mDelegate);
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
    }

    @Test
    public void testDefaultPoses() {
        assertEquals(
                XrPose.create(XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                mManager.getPlayerPanelPose());
    }

    @Test
    public void testQuadMode_PoseUpdates() {
        XrPose newPose =
                XrPose.create(XrVector3.create(1f, 2f, 3f), XrQuaternion.create(0f, 1f, 0f, 0f));

        mManager.onPlayerPanelPoseChanged(newPose);

        assertEquals(newPose, mManager.getPlayerPanelPose());

        // Control panel pose in QUAD mode is positioned relative to the player panel:
        // Y is offset downward by half the layout height (-0.25m), and Z is offset forward by
        // CONTROL_OFFSET_Z (0.04m).
        float expectedControlY = -QUAD_LAYOUT_HEIGHT / 2f;
        XrPose expectedControlPose =
                XrPose.create(
                        XrVector3.create(0f, expectedControlY, CONTROL_OFFSET_Z),
                        XrQuaternion.getIdentity());
        assertEquals(expectedControlPose, mManager.getControlPanelPose());
    }

    @Test
    public void testQuadMode_DragPoseUpdates() {
        XrVector3 startOrigin = XrVector3.create(0f, 0f, 0f);
        XrVector3 startDirection = XrVector3.create(0f, 0f, 1f);

        mManager.onPlayerPanelDragStart(startOrigin, startDirection);

        XrVector3 updateDirection = XrVector3.create(1f, 2f, 1f);
        mManager.onPlayerPanelDragUpdate(startOrigin, updateDirection);

        // Given drag update direction ray (1, 2, 1) and initial distance 0.5m:
        // ||(1, 2, 1)|| = sqrt(6). Projected position is 0.5 * direction / ||direction||.
        float expectedX = (float) (0.5 / Math.sqrt(6.0));
        float expectedY = (float) (1.0 / Math.sqrt(6.0));
        float expectedZ = (float) (0.5 / Math.sqrt(6.0));
        float currentDistance =
                (float)
                        Math.sqrt(
                                expectedX * expectedX
                                        + expectedY * expectedY
                                        + expectedZ * expectedZ);
        float expectedYaw = -expectedX / currentDistance;
        XrPose actualPose = mManager.getPlayerPanelPose();
        assertEquals(expectedX, actualPose.getTranslation().getX(), EPSILON);
        assertEquals(expectedY, actualPose.getTranslation().getY(), EPSILON);
        assertEquals(expectedZ, actualPose.getTranslation().getZ(), EPSILON);
        assertEquals(expectedYaw, actualPose.getRotation().getYaw(), EPSILON);

        mManager.onPlayerPanelDragEnd(startOrigin, updateDirection);
        XrPose endPose = mManager.getPlayerPanelPose();
        assertEquals(expectedX, endPose.getTranslation().getX(), EPSILON);
        assertEquals(expectedY, endPose.getTranslation().getY(), EPSILON);
        assertEquals(expectedZ, endPose.getTranslation().getZ(), EPSILON);
        assertEquals(expectedYaw, endPose.getRotation().getYaw(), EPSILON);
    }

    @Test
    public void testSphereMode_DragPoseUpdates() {
        mManager.updateStrategy(XrSurfaceEntityShape.SPHERE);
        // In SPHERE mode, player panel pose translation should be pinned to origin
        assertEquals(XrPose.getIdentity(), mManager.getPlayerPanelPose());

        when(mDelegate.getLayoutHeight()).thenReturn(SPHERE_LAYOUT_HEIGHT);
        when(mDelegate.getCurveRadius()).thenReturn(SPHERE_CURVE_RADIUS);

        // Ray straight forward (0, 0, -1) from origin (0, 0, 0)
        XrVector3 startOrigin = XrVector3.create(0f, 0f, 0f);
        XrVector3 startDirection = XrVector3.create(0f, 0f, -1f);

        mManager.onPlayerPanelDragStart(startOrigin, startDirection);

        // Ray angled right by 30 degrees
        XrVector3 updateOrigin = XrVector3.create(0f, 0f, 0f);
        float angleRadians = (float) Math.toRadians(30f);
        XrVector3 updateDirection =
                XrVector3.create(
                        (float) Math.sin(angleRadians), 0f, -(float) Math.cos(angleRadians));

        mManager.onPlayerPanelDragUpdate(updateOrigin, updateDirection);

        // Verify player panel rotation is angled left by 60 degrees
        XrQuaternion expectedRotation = getQuaternionFromAxisAngle(0f, 1f, 0f, -60f);
        XrPose expectedPlayerPose = XrPose.create(XrVector3.getZero(), expectedRotation);
        assertEquals(expectedPlayerPose, mManager.getPlayerPanelPose());

        // Verify control panel pose queries (world space, since parent is null in curved mode).
        // Y is offset downward by half the sphere layout height (-0.3m), and Z is at
        // WORLD_CONTROL_OFFSET_Z (0.54m).
        float expectedControlY = -SPHERE_LAYOUT_HEIGHT / 2f;
        XrPose expectedControlPose =
                XrPose.create(XrVector3.create(0f, expectedControlY, WORLD_CONTROL_OFFSET_Z));
        assertEquals(expectedControlPose, mManager.getControlPanelPose());

        // Verify that when switching back to QUAD mode, the player panel starts with its default
        // pose.
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
        assertEquals(
                XrPose.create(XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                mManager.getPlayerPanelPose());
    }

    @Test
    public void testSphereMode_ControlPanelMoveUpdates() {
        mManager.updateStrategy(XrSurfaceEntityShape.SPHERE);
        when(mDelegate.getLayoutHeight()).thenReturn(SPHERE_LAYOUT_HEIGHT);

        // Move control panel in SPHERE mode
        XrVector3 controlTrans = XrVector3.create(5f, 6f, 7f);
        XrQuaternion controlRot = getQuaternionFromAxisAngle(0f, 1f, 0f, 90f);
        XrPose controlPose = XrPose.create(controlTrans, controlRot);

        mManager.onControlPanelPoseChanged(controlPose);

        assertEquals(controlPose, mManager.getControlPanelPose());

        // Verify that when creating a fresh manager, it resets to default pose.
        mManager = new ImmersiveVideoPoseManager(mDelegate);
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
        assertEquals(
                XrPose.create(XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                mManager.getPlayerPanelPose());
    }

    @Test
    public void testHemisphereMode_ControlPanelMoveUpdates() {
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);
        assertEquals(XrPose.getIdentity(), mManager.getPlayerPanelPose());

        XrVector3 controlTrans = XrVector3.create(1f, 2f, 3f);
        XrQuaternion controlRot = getQuaternionFromAxisAngle(0f, 1f, 0f, 90f);
        XrPose controlPose = XrPose.create(controlTrans, controlRot);

        mManager.onControlPanelPoseChanged(controlPose);

        // In HEMISPHERE mode, when the control panel moves to (1, 2, 3) rotated +90 deg around Y:
        // 1. The unrotated local offset from control panel to player panel is:
        //    localX = 0, localY = QUAD_LAYOUT_HEIGHT / 2 (+0.25m), localZ = -WORLD_CONTROL_OFFSET_Z
        // (-0.54m).
        // 2. Rotating (0, 0.25, -0.54) by +90 deg around Y transforms (x, y, z) -> (z, y, -x):
        //    rotatedX = -0.54m, rotatedY = +0.25m, rotatedZ = 0m.
        // 3. Adding to controlTrans (1, 2, 3):
        //    expectedX = 1 - 0.54 = 0.46m, expectedY = 2 + 0.25 = 2.25m, expectedZ = 3 + 0 = 3.00m.
        float localOffsetY = QUAD_LAYOUT_HEIGHT / 2f;
        float expectedPlayerX = controlTrans.getX() - WORLD_CONTROL_OFFSET_Z;
        float expectedPlayerY = controlTrans.getY() + localOffsetY;
        float expectedPlayerZ = controlTrans.getZ();

        XrPose actualPose = mManager.getPlayerPanelPose();
        assertEquals(expectedPlayerX, actualPose.getTranslation().getX(), EPSILON);
        assertEquals(expectedPlayerY, actualPose.getTranslation().getY(), EPSILON);
        assertEquals(expectedPlayerZ, actualPose.getTranslation().getZ(), EPSILON);
        assertEquals(controlRot, actualPose.getRotation());
        assertEquals(controlPose, mManager.getControlPanelPose());

        XrVector3 dragTrans = XrVector3.create(10f, 11f, 12f);
        XrQuaternion dragRot = getQuaternionFromAxisAngle(1f, 0f, 0f, 180f);
        mManager.onPlayerPanelPoseChanged(XrPose.create(dragTrans, dragRot));

        // Dragging/moving the player panel in HEMISPHERE mode is ignored;
        // player panel pose remains tied to the control panel pose.
        XrPose endPose = mManager.getPlayerPanelPose();
        assertEquals(expectedPlayerX, endPose.getTranslation().getX(), EPSILON);
        assertEquals(expectedPlayerY, endPose.getTranslation().getY(), EPSILON);
        assertEquals(expectedPlayerZ, endPose.getTranslation().getZ(), EPSILON);
    }

    @Test
    public void testHemisphereMode_ControlPanelMoveUpdatesWithXRotation() {
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);

        XrVector3 controlTrans = XrVector3.create(1f, 2f, 3f);
        XrQuaternion controlRot = getQuaternionFromAxisAngle(1f, 0f, 0f, 90f);
        XrPose controlPose = XrPose.create(controlTrans, controlRot);

        mManager.onControlPanelPoseChanged(controlPose);

        // Rotating by +90 deg around X transforms (x, y, z) -> (x, -z, y):
        // rotatedX = 0m, rotatedY = +WORLD_CONTROL_OFFSET_Z (+0.54m), rotatedZ = +localOffsetY
        // (+0.25m).
        float localOffsetY = QUAD_LAYOUT_HEIGHT / 2f;
        float expectedPlayerX = controlTrans.getX();
        float expectedPlayerY = controlTrans.getY() + WORLD_CONTROL_OFFSET_Z;
        float expectedPlayerZ = controlTrans.getZ() + localOffsetY;

        XrPose actualPose = mManager.getPlayerPanelPose();
        assertEquals(expectedPlayerX, actualPose.getTranslation().getX(), EPSILON);
        assertEquals(expectedPlayerY, actualPose.getTranslation().getY(), EPSILON);
        assertEquals(expectedPlayerZ, actualPose.getTranslation().getZ(), EPSILON);
        assertEquals(controlRot, actualPose.getRotation());
    }

    private XrQuaternion getQuaternionFromAxisAngle(
            float ax, float ay, float az, float angleDegrees) {
        float angleRadians = (float) Math.toRadians(angleDegrees);
        float sinHalf = (float) Math.sin(angleRadians / 2.0);
        float cosHalf = (float) Math.cos(angleRadians / 2.0);
        return XrQuaternion.create(ax * sinHalf, ay * sinHalf, az * sinHalf, cosHalf);
    }
}
