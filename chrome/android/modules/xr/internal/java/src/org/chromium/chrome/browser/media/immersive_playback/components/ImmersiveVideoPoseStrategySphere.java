// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrVector3;

/**
 * Pose management strategy for sphere projection mode. Assumes the control panel is not parented to
 * the player panel and is positioned independently in world space.
 */
@NullMarked
class ImmersiveVideoPoseStrategySphere implements ImmersiveVideoPoseStrategy {
    private static final float DRAG_SENSITIVITY = 2.0f;
    private static final XrVector3 CONTROL_OFFSET_Z =
            ImmersiveVideoPoseStrategyQuad.DEFAULT_PLAYER_TRANSLATION;

    private final ImmersiveVideoPoseManager.Delegate mDelegate;
    private @Nullable XrPose mControlPose;
    private float mCurrentYaw;
    private float mDragStartYaw;
    private float mStartOffsetYaw;

    public ImmersiveVideoPoseStrategySphere(ImmersiveVideoPoseManager.Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onPlayerPanelPoseChanged(XrPose pose) {
        mCurrentYaw = pose.getRotation().getYaw();
    }

    @Override
    public void onControlPanelPoseChanged(XrPose pose) {
        mControlPose = pose;
        mCurrentYaw = pose.getRotation().getYaw();
    }

    @Override
    public void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {
        mDragStartYaw = calculateYawFromRay(origin, direction, mDelegate.getCurveRadius());
        mStartOffsetYaw = mCurrentYaw;
    }

    @Override
    public void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {
        updateDragYaw(origin, direction);
    }

    @Override
    public void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {
        updateDragYaw(origin, direction);
    }

    private void updateDragYaw(XrVector3 origin, XrVector3 direction) {
        float currentDragYaw = calculateYawFromRay(origin, direction, mDelegate.getCurveRadius());
        float deltaYaw = currentDragYaw - mDragStartYaw;
        mCurrentYaw = mStartOffsetYaw - deltaYaw * DRAG_SENSITIVITY;
    }

    @Override
    public XrPose getPlayerPanelPose() {
        return XrPose.create(XrVector3.getZero(), XrQuaternion.fromYaw(mCurrentYaw));
    }

    @Override
    public XrPose getControlPanelPose() {
        return mControlPose != null ? mControlPose : XrPose.create(getOffset());
    }

    private XrVector3 getOffset() {
        return XrVector3.create(0f, -mDelegate.getLayoutHeight() / 2f, 0f).plus(CONTROL_OFFSET_Z);
    }

    /**
     * Calculates the yaw angle (rotation around the Y axis) of the point where a ray intersects a
     * sphere of a given radius centered at the origin.
     *
     * <p>The ray is defined by origin O and direction D. A point on the ray is P(t) = O + t*D. We
     * solve for the intersection with the sphere of radius R: ||O + t*D||^2 = R^2
     *
     * <p>Assuming D is normalized (||D|| = 1), this expands to the quadratic equation: t^2 +
     * 2*t*(O.D) + (||O||^2 - R^2) = 0
     *
     * <p>Using the quadratic formula, the positive intersection distance t is: t = -(O.D) +
     * sqrt((O.D)^2 - (||O||^2 - R^2))
     *
     * <p>The intersection point is P = O + t*D. The yaw angle is then calculated in the XZ plane:
     * yaw = atan2(P.x, -P.z)
     *
     * @param origin The origin of the ray (O).
     * @param direction The direction of the ray (D).
     * @param radius The radius of the sphere (R).
     * @return The yaw angle in radians.
     */
    private float calculateYawFromRay(XrVector3 origin, XrVector3 direction, float radius) {
        XrVector3 d = direction.toNormalized();
        float oDotD = origin.dot(d);
        float oLenSq = origin.dot(origin);
        float c = oLenSq - radius * radius;

        float discriminant = oDotD * oDotD - c;
        if (discriminant < 0) {
            return 0f;
        }

        float t = -oDotD + (float) Math.sqrt(discriminant);
        if (t < 0) {
            return 0f;
        }

        XrVector3 p = origin.plus(d.times(t));
        return (float) Math.atan2(p.getX(), -p.getZ());
    }
}
