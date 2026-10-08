// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPose;

/**
 * Pose management strategy for HEMISPHERE (180 curved) projection mode. Positions the control panel
 * following the hemisphere's pose and rotation.
 */
@NullMarked
class ImmersiveVideoPoseStrategyHemisphere extends ImmersiveVideoPoseStrategySphere {
    public ImmersiveVideoPoseStrategyHemisphere(ImmersiveVideoPoseManager.Delegate delegate) {
        super(delegate);
    }

    @Override
    public XrPose getControlPanelPose() {
        XrPose playerPose = getPlayerPanelPose();
        return XrPose.create(playerPose.transformPoint(getOffset()), playerPose.getRotation());
    }
}
