// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Strategy interface for projection-specific spatial coordinates management. */
@NullMarked
interface ImmersiveVideoPoseStrategy {
    void onPlayerPanelPoseChanged(XrPose pose);

    void onControlPanelPoseChanged(XrPose pose);

    default void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {}

    default void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {}

    default void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {}

    XrPose getPlayerPanelPose();

    XrPose getControlPanelPose();
}
