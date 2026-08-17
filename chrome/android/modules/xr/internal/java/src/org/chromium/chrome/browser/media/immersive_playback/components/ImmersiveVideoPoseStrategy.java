// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Strategy interface for projection-specific spatial coordinates management. */
@NullMarked
interface ImmersiveVideoPoseStrategy {
    /** Sets the anchor pose for the strategy. */
    void setAnchorPose(@Nullable XrPose anchorPose);

    /** Called when the player panel pose is updated. */
    void onPlayerPanelPoseChanged(XrPose pose);

    /** Called when the control panel pose is updated. */
    default void onControlPanelPoseChanged(XrPose pose) {}

    /** Called when the player panel drag starts. */
    default void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {}

    /** Called when the player panel drag is updated. */
    default void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {}

    /** Called when the player panel drag ends. */
    default void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {}

    /** Returns the current player panel pose. */
    XrPose getPlayerPanelPose();

    /** Returns the current control panel pose. */
    XrPose getControlPanelPose();
}
