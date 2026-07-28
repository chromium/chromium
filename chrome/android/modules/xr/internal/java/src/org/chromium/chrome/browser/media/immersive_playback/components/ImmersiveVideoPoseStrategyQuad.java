// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrVector3;

/**
 * Pose management strategy for QUAD (flat screen) projection mode. Assumes the control panel is
 * parented to the player panel.
 */
@NullMarked
class ImmersiveVideoPoseStrategyQuad implements ImmersiveVideoPoseStrategy {
    static final XrVector3 DEFAULT_PLAYER_TRANSLATION = XrVector3.create(0f, 0f, 0.5f);
    private final ImmersiveVideoPoseManager.Delegate mDelegate;
    private final XrPose mPlayerPose;

    public ImmersiveVideoPoseStrategyQuad(ImmersiveVideoPoseManager.Delegate delegate) {
        mDelegate = delegate;
        mPlayerPose = XrPose.create(DEFAULT_PLAYER_TRANSLATION);
    }

    @Override
    public void onPlayerPanelPoseChanged(XrPose pose) {}

    @Override
    public void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {}

    @Override
    public void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {}

    @Override
    public void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {}

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
        return XrVector3.create(0f, -mDelegate.getLayoutHeight() / 2f, 0f);
    }
}
