// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Helper class that manages 3D spatial coordinate calculations and center screen tracking. */
@NullMarked
public class ImmersiveVideoPoseManager {
    /** Delegate for providing layout dimensions needed for vertical offset calculations. */
    public interface Delegate {
        /** Returns the layout height of the video surface. */
        float getLayoutHeight();
    }

    private XrPose mCenterPose = XrPose.create(XrVector3.create(0f, 0f, 0.5f));
    private final Delegate mDelegate;

    /**
     * Creates a new {@link ImmersiveVideoPoseManager}.
     *
     * @param delegate The {@link Delegate}.
     */
    public ImmersiveVideoPoseManager(Delegate delegate) {
        mDelegate = delegate;
    }

    /** Called when the pose of the player panel changes during interaction. */
    public void onPlayerPanelPoseChanged(XrPose pose, @ImmersiveProjectionType int projectionType) {
        if (projectionType == ImmersiveProjectionType.QUAD) {
            mCenterPose = pose;
        }
    }

    /** Called when the pose of the control panel changes during interaction. */
    public void onControlPanelPoseChanged(
            XrPose pose, @ImmersiveProjectionType int projectionType) {
        if (projectionType != ImmersiveProjectionType.QUAD) {
            XrVector3 translation = pose.getTranslation();
            mCenterPose =
                    XrPose.create(
                            XrVector3.create(
                                    translation.getX(),
                                    translation.getY() - getVerticalOffset(),
                                    translation.getZ()),
                            pose.getRotation());
        }
    }

    /** Returns the expected pose for the player panel based on the current projection mode. */
    public XrPose getPlayerPanelPose(@ImmersiveProjectionType int projectionType) {
        return projectionType == ImmersiveProjectionType.QUAD ? mCenterPose : XrPose.getIdentity();
    }

    /** Returns the expected pose for the control panel based on the current projection mode. */
    public XrPose getControlPanelPose(@ImmersiveProjectionType int projectionType) {
        float verticalOffset = getVerticalOffset();
        if (projectionType == ImmersiveProjectionType.QUAD) {
            return XrPose.create(XrVector3.create(0f, verticalOffset, 0f));
        } else {
            XrVector3 centerTranslation = mCenterPose.getTranslation();
            return XrPose.create(
                    XrVector3.create(
                            centerTranslation.getX(),
                            centerTranslation.getY() + verticalOffset,
                            centerTranslation.getZ()),
                    mCenterPose.getRotation());
        }
    }

    private float getVerticalOffset() {
        return -mDelegate.getLayoutHeight() / 2f;
    }
}
