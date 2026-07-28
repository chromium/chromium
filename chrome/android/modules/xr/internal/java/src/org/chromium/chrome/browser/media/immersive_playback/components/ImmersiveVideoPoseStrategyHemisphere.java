// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrVector3;

/**
 * Pose management strategy for HEMISPHERE projection mode. The player panel follows the control
 * panel when the control panel is moved. It behaves similarly to QUAD but allows dragging the
 * control panel in world space.
 */
@NullMarked
class ImmersiveVideoPoseStrategyHemisphere implements ImmersiveVideoPoseStrategy {
    private static final XrVector3 CONTROL_OFFSET_Z =
            ImmersiveVideoPoseStrategyQuad.DEFAULT_PLAYER_TRANSLATION.plus(
                    ImmersiveVideoPoseStrategyQuad.CONTROL_OFFSET_Z);

    private final ImmersiveVideoPoseManager.Delegate mDelegate;
    private XrPose mPlayerPose;

    public ImmersiveVideoPoseStrategyHemisphere(ImmersiveVideoPoseManager.Delegate delegate) {
        mDelegate = delegate;
        mPlayerPose = XrPose.getIdentity();
    }

    @Override
    public void onPlayerPanelPoseChanged(XrPose pose) {}

    @Override
    public void onControlPanelPoseChanged(XrPose pose) {
        XrQuaternion rotation = pose.getRotation();
        XrVector3 offset = getOffset(rotation);
        mPlayerPose = XrPose.create(pose.getTranslation().plus(offset), rotation);
    }

    @Override
    public XrPose getPlayerPanelPose() {
        return mPlayerPose;
    }

    @Override
    public XrPose getControlPanelPose() {
        XrQuaternion rotation = mPlayerPose.getRotation();
        XrVector3 offset = getOffset(rotation);
        return XrPose.create(mPlayerPose.getTranslation().minus(offset), rotation);
    }

    private XrVector3 getOffset(XrQuaternion rotation) {
        XrVector3 localOffset =
                XrVector3.create(0f, mDelegate.getLayoutHeight() / 2f, 0f).minus(CONTROL_OFFSET_Z);
        return rotation.rotate(localOffset);
    }
}
