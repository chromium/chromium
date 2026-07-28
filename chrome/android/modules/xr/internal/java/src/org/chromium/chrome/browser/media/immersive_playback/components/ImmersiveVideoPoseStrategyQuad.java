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
 * Pose management strategy for QUAD (flat screen) projection mode. Assumes the control panel is
 * parented to the player panel.
 */
@NullMarked
class ImmersiveVideoPoseStrategyQuad implements ImmersiveVideoPoseStrategy {
    static final XrVector3 DEFAULT_PLAYER_TRANSLATION = XrVector3.create(0f, 0f, 0.5f);
    static final XrVector3 CONTROL_OFFSET_Z = XrVector3.create(0f, 0f, 0.04f);
    // Emulates MovableComponent's OpenXR FLAG_SCALE_WITH_DISTANCE depth amplification.
    private static final float DEPTH_SENSITIVITY = 3.0f;
    private final ImmersiveVideoPoseManager.Delegate mDelegate;
    private XrPose mPlayerPose;
    private @Nullable XrPose mDragStartPlayerPose;
    private float mHitPointToOriginDistance;
    private @Nullable XrVector3 mGrabPointToCenterOffset;
    private @Nullable XrVector3 mStartOrigin;

    public ImmersiveVideoPoseStrategyQuad(ImmersiveVideoPoseManager.Delegate delegate) {
        mDelegate = delegate;
        mPlayerPose = XrPose.create(DEFAULT_PLAYER_TRANSLATION);
    }

    @Override
    public void onPlayerPanelPoseChanged(XrPose pose) {
        mPlayerPose = pose;
    }

    @Override
    public void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {
        mDragStartPlayerPose = mPlayerPose;
        mStartOrigin = origin;
        XrVector3 normalizedDirection = direction.toNormalized();
        XrVector3 initialTranslation = mDragStartPlayerPose.getTranslation();
        XrVector3 originToTranslation = initialTranslation.minus(origin);
        // Calculate initial distance along the ray vector.
        mHitPointToOriginDistance = originToTranslation.dot(normalizedDirection);
        if (mHitPointToOriginDistance <= 0f) {
            mHitPointToOriginDistance = originToTranslation.getLength();
        }
        // Save the offset vector between the ray grab point and the panel center.
        XrVector3 initialGrabPoint =
                origin.plus(normalizedDirection.times(mHitPointToOriginDistance));
        mGrabPointToCenterOffset = initialTranslation.minus(initialGrabPoint);
    }

    @Override
    public void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {
        updateDrag(origin, direction);
    }

    @Override
    public void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {
        updateDrag(origin, direction);
        mDragStartPlayerPose = null;
        mGrabPointToCenterOffset = null;
        mStartOrigin = null;
    }

    private void updateDrag(XrVector3 origin, XrVector3 direction) {
        if (mDragStartPlayerPose == null
                || mGrabPointToCenterOffset == null
                || mStartOrigin == null
                || mHitPointToOriginDistance <= 0f) {
            return;
        }

        XrVector3 normalizedDirection = direction.toNormalized();

        // Scale depth displacement along ray to match MovableComponent's
        // FLAG_SCALE_WITH_DISTANCE.
        XrVector3 originDelta = origin.minus(mStartOrigin);
        float depthOffset = originDelta.dot(normalizedDirection) * (DEPTH_SENSITIVITY - 1.0f);
        float currentDistance = mHitPointToOriginDistance + depthOffset;

        // Position panel along the ray preserving relative grab offset.
        XrVector3 grabPoint = origin.plus(normalizedDirection.times(currentDistance));
        XrVector3 proposedTranslation = grabPoint.plus(mGrabPointToCenterOffset);

        // Turn panel gently toward origin as it translates left/right, scaling linearly with arc
        // displacement.
        float yaw = -proposedTranslation.getX() / currentDistance;
        XrQuaternion newRotation = XrQuaternion.fromYaw(yaw);
        mPlayerPose = XrPose.create(proposedTranslation, newRotation);
    }

    @Override
    public void onControlPanelPoseChanged(XrPose pose) {}

    @Override
    public XrPose getPlayerPanelPose() {
        return mPlayerPose;
    }

    @Override
    public XrPose getControlPanelPose() {
        return XrPose.create(getOffset());
    }

    private XrVector3 getOffset() {
        return XrVector3.create(0f, -mDelegate.getLayoutHeight() / 2f, 0f).plus(CONTROL_OFFSET_Z);
    }
}
