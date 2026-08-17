// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrPose;

/**
 * Pose management strategy for HEMISPHERE (180 curved) projection mode. Sets initial yaw once when
 * mCurrentYaw is null to position the hemisphere in front of the user, while preserving user
 * rotations on subsequent anchor pose updates.
 */
@NullMarked
class ImmersiveVideoPoseStrategyHemisphere extends ImmersiveVideoPoseStrategySphere {
    public ImmersiveVideoPoseStrategyHemisphere(ImmersiveVideoPoseManager.Delegate delegate) {
        super(delegate);
    }

    @Override
    public void setAnchorPose(@Nullable XrPose anchorPose) {
        mAnchorPose = anchorPose != null ? anchorPose : XrPose.getIdentity();
        if (mCurrentYaw == null) {
            mCurrentYaw = mAnchorPose.getRotation().getYaw();
        }
    }

    @Override
    public XrPose getControlPanelPose() {
        XrPose playerPose = getPlayerPanelPose();
        return XrPose.create(playerPose.transformPoint(getOffset()), playerPose.getRotation());
    }
}
