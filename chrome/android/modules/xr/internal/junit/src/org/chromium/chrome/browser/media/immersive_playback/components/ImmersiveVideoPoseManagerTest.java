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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Tests for {@link ImmersiveVideoPoseManager}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    private static final float DEFAULT_PLAYER_Z = -1.5f;
    // Forward Z offset of the control panel relative to the player panel.
    private static final float CONTROL_OFFSET_Z = 0.04f;
    // In SPHERE/HEMISPHERE mode, the default control panel Z in unrotated world space is
    // DEFAULT_PLAYER_Z + CONTROL_OFFSET_Z (-1.46m).
    private static final float WORLD_CONTROL_OFFSET_Z = DEFAULT_PLAYER_Z + CONTROL_OFFSET_Z;

    private static final XrVector3 ANCHOR_TRANSLATION = XrVector3.create(1f, 2f, 3f);
    private static final float ANCHOR_YAW_DEGREES = 90f;
    private static final float ANCHOR_YAW_RADIANS = (float) Math.toRadians(ANCHOR_YAW_DEGREES);
    private static final XrPose ANCHOR_POSE =
            XrPose.create(ANCHOR_TRANSLATION, XrQuaternion.fromYaw(ANCHOR_YAW_RADIANS));

    @Mock private ImmersiveVideoPoseManager.Delegate mDelegate;

    private ImmersiveVideoPoseManager mManager;

    @Before
    public void setUp() {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        when(mDelegate.getLayoutHeight()).thenReturn(QUAD_LAYOUT_HEIGHT);
        when(mDelegate.getCurveRadius()).thenReturn(DEFAULT_CURVE_RADIUS);
        mManager = new ImmersiveVideoPoseManager(mDelegate);
        mManager.setAnchorPose(XrPose.create(XrVector3.getZero()));
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
        XrVector3 startDirection = XrVector3.create(0f, 0f, -1f);

        mManager.onPlayerPanelDragStart(startOrigin, startDirection);

        XrVector3 updateDirection = XrVector3.create(1f, 2f, -1f);
        mManager.onPlayerPanelDragUpdate(startOrigin, updateDirection);

        // Given drag update direction ray (1, 2, -1) and initial distance 1.5m:
        // ||(1, 2, -1)|| = sqrt(6). Projected position is 1.5 * direction / ||direction||.
        float expectedX = (float) (1.5 / Math.sqrt(6.0));
        float expectedY = (float) (3.0 / Math.sqrt(6.0));
        float expectedZ = -(float) (1.5 / Math.sqrt(6.0));
        float currentDistance =
                (float)
                        Math.sqrt(
                                expectedX * expectedX
                                        + expectedY * expectedY
                                        + expectedZ * expectedZ);
        float expectedYaw = -expectedX / currentDistance;
        XrPose expectedPose =
                XrPose.create(
                        XrVector3.create(expectedX, expectedY, expectedZ),
                        XrQuaternion.fromYaw(expectedYaw));
        assertPoseEquals(expectedPose, mManager.getPlayerPanelPose());

        mManager.onPlayerPanelDragEnd(startOrigin, updateDirection);
        assertPoseEquals(expectedPose, mManager.getPlayerPanelPose());
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

        // Verify control panel pose in SPHERE mode does not follow sphere rotation.
        float radius = Math.abs(WORLD_CONTROL_OFFSET_Z);
        float expectedControlY = -SPHERE_LAYOUT_HEIGHT / 2f;
        XrVector3 expectedControlTrans = XrVector3.create(0f, expectedControlY, -radius);
        XrPose expectedControlPose =
                XrPose.create(expectedControlTrans, XrQuaternion.getIdentity());
        assertEquals(expectedControlPose, mManager.getControlPanelPose());

        // Verify that when switching back to QUAD mode, the player panel starts with its default
        // pose.
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
        assertEquals(
                XrPose.create(XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                mManager.getPlayerPanelPose());
    }

    @Test
    public void testHemisphereMode_PoseUpdates() {
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);
        assertEquals(XrPose.getIdentity(), mManager.getPlayerPanelPose());

        XrPose newPose =
                XrPose.create(
                        XrVector3.create(0f, 0f, 0f),
                        XrQuaternion.fromYaw((float) Math.toRadians(45f)));
        mManager.onPlayerPanelPoseChanged(newPose);

        assertEquals(newPose, mManager.getPlayerPanelPose());

        // Verify control panel pose in HEMISPHERE mode follows hemisphere rotation.
        float radius = Math.abs(WORLD_CONTROL_OFFSET_Z);
        float expectedControlYaw = (float) Math.toRadians(45f);
        float expectedControlX = -(float) (radius * Math.sin(expectedControlYaw));
        float expectedControlZ = -(float) (radius * Math.cos(expectedControlYaw));
        float expectedControlY = -QUAD_LAYOUT_HEIGHT / 2f;
        XrPose expectedControlPose =
                XrPose.create(
                        XrVector3.create(expectedControlX, expectedControlY, expectedControlZ),
                        XrQuaternion.fromYaw(expectedControlYaw));
        assertPoseEquals(expectedControlPose, mManager.getControlPanelPose());
    }

    @Test
    public void testQuadMode_AnchorPose() {
        mManager.setAnchorPose(ANCHOR_POSE);

        XrPose expectedPlayerPose =
                XrPose.create(
                        ANCHOR_POSE.transformPoint(
                                XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                        ANCHOR_POSE.getRotation());
        assertPoseEquals(expectedPlayerPose, mManager.getPlayerPanelPose());

        // Setting anchor pose to null defaults back to identity anchor.
        mManager.setAnchorPose(null);
        assertEquals(
                XrPose.create(XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                mManager.getPlayerPanelPose());
    }

    @Test
    public void testSphereMode_AnchorPoseAndDecoupledControl() {
        mManager.updateStrategy(XrSurfaceEntityShape.SPHERE);
        when(mDelegate.getLayoutHeight()).thenReturn(SPHERE_LAYOUT_HEIGHT);

        mManager.setAnchorPose(ANCHOR_POSE);

        // Player panel translation is placed at the anchor translation, with anchor yaw.
        assertEquals(ANCHOR_TRANSLATION, mManager.getPlayerPanelPose().getTranslation());
        assertEquals(
                ANCHOR_YAW_RADIANS,
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);

        // Control panel pose is computed from the anchor pose.
        XrPose expectedControlPose =
                XrPose.create(
                        ANCHOR_POSE.transformPoint(
                                XrVector3.create(
                                        0f, -SPHERE_LAYOUT_HEIGHT / 2f, WORLD_CONTROL_OFFSET_Z)),
                        ANCHOR_POSE.getRotation());
        assertPoseEquals(expectedControlPose, mManager.getControlPanelPose());

        // Rotate the sphere player panel (e.g. user rotates video sphere).
        mManager.onPlayerPanelPoseChanged(
                XrPose.create(
                        ANCHOR_TRANSLATION,
                        XrQuaternion.fromYaw((float) Math.toRadians(120f))));
        assertEquals(
                (float) Math.toRadians(120f),
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);

        // Verify control panel pose in SPHERE mode does NOT follow sphere rotation.
        assertEquals(
                expectedControlPose.getRotation().getYaw(),
                mManager.getControlPanelPose().getRotation().getYaw(),
                EPSILON);

        // Updating anchor pose updates the control panel to the new anchor yaw.
        XrPose newAnchor =
                XrPose.create(
                        XrVector3.create(0f, 0f, 0f),
                        XrQuaternion.fromYaw((float) Math.toRadians(60f)));
        mManager.setAnchorPose(newAnchor);
        assertEquals(
                (float) Math.toRadians(60f),
                mManager.getControlPanelPose().getRotation().getYaw(),
                EPSILON);
    }

    @Test
    public void testHemisphereMode_AnchorPoseYawPreservation() {
        mManager = new ImmersiveVideoPoseManager(mDelegate);
        // Initial anchor pose sets initial yaw.
        mManager.setAnchorPose(ANCHOR_POSE);
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);

        assertEquals(ANCHOR_TRANSLATION, mManager.getPlayerPanelPose().getTranslation());
        assertEquals(
                ANCHOR_YAW_RADIANS,
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);

        // User rotates the hemisphere to 120 degrees.
        mManager.onPlayerPanelPoseChanged(
                XrPose.create(
                        ANCHOR_TRANSLATION,
                        XrQuaternion.fromYaw((float) Math.toRadians(120f))));
        assertEquals(
                (float) Math.toRadians(120f),
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);
        // In HEMISPHERE mode, control panel follows hemisphere rotation.
        assertEquals(
                (float) Math.toRadians(120f),
                mManager.getControlPanelPose().getRotation().getYaw(),
                EPSILON);

        // Subsequent anchor pose update (e.g. head movement / recenter) updates translation but
        // preserves the user's yaw rotation.
        XrPose secondAnchor =
                XrPose.create(
                        XrVector3.create(2f, 2f, 2f),
                        XrQuaternion.fromYaw(0f));
        mManager.setAnchorPose(secondAnchor);

        assertEquals(secondAnchor.getTranslation(), mManager.getPlayerPanelPose().getTranslation());
        assertEquals(
                (float) Math.toRadians(120f),
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);
        assertEquals(
                (float) Math.toRadians(120f),
                mManager.getControlPanelPose().getRotation().getYaw(),
                EPSILON);
    }

    @Test
    public void testHemisphereMode_DragUpdatesRotateBothPlayerAndControl() {
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);
        when(mDelegate.getLayoutHeight()).thenReturn(QUAD_LAYOUT_HEIGHT);
        when(mDelegate.getCurveRadius()).thenReturn(DEFAULT_CURVE_RADIUS);

        XrVector3 startOrigin = XrVector3.create(0f, 0f, 0f);
        XrVector3 startDirection = XrVector3.create(0f, 0f, -1f);
        mManager.onPlayerPanelDragStart(startOrigin, startDirection);

        // Ray angled right by 30 degrees
        float angleRadians = (float) Math.toRadians(30f);
        XrVector3 updateDirection =
                XrVector3.create(
                        (float) Math.sin(angleRadians), 0f, -(float) Math.cos(angleRadians));
        mManager.onPlayerPanelDragUpdate(startOrigin, updateDirection);

        float expectedYaw = (float) Math.toRadians(-60f);
        assertEquals(expectedYaw, mManager.getPlayerPanelPose().getRotation().getYaw(), EPSILON);
        assertEquals(expectedYaw, mManager.getControlPanelPose().getRotation().getYaw(), EPSILON);
    }

    @Test
    public void testStrategySwitching_PreservesAnchorPose() {
        mManager.setAnchorPose(ANCHOR_POSE);

        // Switch to SPHERE
        mManager.updateStrategy(XrSurfaceEntityShape.SPHERE);
        assertEquals(ANCHOR_TRANSLATION, mManager.getPlayerPanelPose().getTranslation());
        assertEquals(
                ANCHOR_YAW_RADIANS,
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);

        // Switch to HEMISPHERE
        mManager.updateStrategy(XrSurfaceEntityShape.HEMISPHERE);
        assertEquals(ANCHOR_TRANSLATION, mManager.getPlayerPanelPose().getTranslation());
        assertEquals(
                ANCHOR_YAW_RADIANS,
                mManager.getPlayerPanelPose().getRotation().getYaw(),
                EPSILON);

        // Switch to QUAD
        mManager.updateStrategy(XrSurfaceEntityShape.QUAD);
        XrPose expectedQuadPlayerPose =
                XrPose.create(
                        ANCHOR_POSE.transformPoint(
                                XrVector3.create(0f, 0f, DEFAULT_PLAYER_Z)),
                        ANCHOR_POSE.getRotation());
        assertPoseEquals(expectedQuadPlayerPose, mManager.getPlayerPanelPose());
    }

    private void assertVectorEquals(XrVector3 expected, XrVector3 actual) {
        assertEquals(expected.getX(), actual.getX(), EPSILON);
        assertEquals(expected.getY(), actual.getY(), EPSILON);
        assertEquals(expected.getZ(), actual.getZ(), EPSILON);
    }

    private void assertPoseEquals(XrPose expected, XrPose actual) {
        assertVectorEquals(expected.getTranslation(), actual.getTranslation());
        assertEquals(expected.getRotation().getYaw(), actual.getRotation().getYaw(), EPSILON);
    }

    private XrQuaternion getQuaternionFromAxisAngle(
            float ax, float ay, float az, float angleDegrees) {
        float angleRadians = (float) Math.toRadians(angleDegrees);
        float sinHalf = (float) Math.sin(angleRadians / 2.0);
        float cosHalf = (float) Math.cos(angleRadians / 2.0);
        return XrQuaternion.create(ax * sinHalf, ay * sinHalf, az * sinHalf, cosHalf);
    }
}
