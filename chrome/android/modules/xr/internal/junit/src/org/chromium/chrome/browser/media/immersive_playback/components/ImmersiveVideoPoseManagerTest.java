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
        assertEquals(XrPose.create(XrVector3.create(0f, 0f, 0.5f)), mManager.getPlayerPanelPose());
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

        // Verify control panel pose queries (world space, since parent is null in curved mode)
        XrPose expectedControlPose = XrPose.create(XrVector3.create(0f, -0.3f, 0.51f));
        assertEquals(expectedControlPose, mManager.getControlPanelPose());

        // Verify that when switching back to QUAD mode, the player panel starts with its default
        // pose
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
        assertEquals(XrPose.create(XrVector3.create(0f, 0f, 0.5f)), mManager.getPlayerPanelPose());
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

        // Verify that when creating a fresh manager, it resets to default pose
        mManager = new ImmersiveVideoPoseManager(mDelegate);
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
        assertEquals(XrPose.create(XrVector3.create(0f, 0f, 0.5f)), mManager.getPlayerPanelPose());
    }

    @Test
    public void testHemisphereMode_ControlPanelMoveUpdates() {
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);
        assertEquals(XrPose.getIdentity(), mManager.getPlayerPanelPose());

        XrVector3 controlTrans = XrVector3.create(1f, 2f, 3f);
        XrQuaternion controlRot = getQuaternionFromAxisAngle(0f, 1f, 0f, 90f);
        XrPose controlPose = XrPose.create(controlTrans, controlRot);

        mManager.onControlPanelPoseChanged(controlPose);

        XrPose actualPose = mManager.getPlayerPanelPose();
        assertEquals(0.46f, actualPose.getTranslation().getX(), EPSILON);
        assertEquals(2.25f, actualPose.getTranslation().getY(), EPSILON);
        assertEquals(3.00f, actualPose.getTranslation().getZ(), EPSILON);
        assertEquals(controlRot, actualPose.getRotation());
        assertEquals(controlPose, mManager.getControlPanelPose());

        XrVector3 dragTrans = XrVector3.create(10f, 11f, 12f);
        XrQuaternion dragRot = getQuaternionFromAxisAngle(1f, 0f, 0f, 180f);
        mManager.onPlayerPanelPoseChanged(XrPose.create(dragTrans, dragRot));

        XrPose endPose = mManager.getPlayerPanelPose();
        assertEquals(0.46f, endPose.getTranslation().getX(), EPSILON);
        assertEquals(2.25f, endPose.getTranslation().getY(), EPSILON);
        assertEquals(3.00f, endPose.getTranslation().getZ(), EPSILON);
    }

    @Test
    public void testHemisphereMode_ControlPanelMoveUpdatesWithXRotation() {
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);

        XrVector3 controlTrans = XrVector3.create(1f, 2f, 3f);
        XrQuaternion controlRot = getQuaternionFromAxisAngle(1f, 0f, 0f, 90f);
        XrPose controlPose = XrPose.create(controlTrans, controlRot);

        mManager.onControlPanelPoseChanged(controlPose);

        XrPose actualPose = mManager.getPlayerPanelPose();
        assertEquals(1.00f, actualPose.getTranslation().getX(), EPSILON);
        assertEquals(2.54f, actualPose.getTranslation().getY(), EPSILON);
        assertEquals(3.25f, actualPose.getTranslation().getZ(), EPSILON);
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
