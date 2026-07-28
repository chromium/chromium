// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Coordinates spatial layout calculations by delegating to projection-specific strategies. */
@NullMarked
public class ImmersiveVideoPoseManager implements ImmersiveVideoPoseStrategy {
    /** Delegate for providing layout dimensions needed for vertical offset calculations. */
    public interface Delegate {
        /** Returns the layout height of the video surface. */
        float getLayoutHeight();

        /** Returns the radius of the curved surface. */
        float getCurveRadius();
    }

    private final Delegate mDelegate;
    private @Nullable ImmersiveVideoPoseStrategy mStrategy;

    public ImmersiveVideoPoseManager(Delegate delegate) {
        mDelegate = delegate;
    }

    /** Updates the active projection strategy. */
    public void updateStrategy(@XrSurfaceEntityShape int shape) {
        mStrategy = createStrategy(shape);
    }

    private ImmersiveVideoPoseStrategy createStrategy(@XrSurfaceEntityShape int shape) {
        if (shape == XrSurfaceEntityShape.QUAD) {
            return new ImmersiveVideoPoseStrategyQuad(mDelegate);
        } else if (shape == XrSurfaceEntityShape.HEMISPHERE) {
            return new ImmersiveVideoPoseStrategyHemisphere(mDelegate);
        } else {
            return new ImmersiveVideoPoseStrategySphere(mDelegate);
        }
    }

    @Override
    public void onPlayerPanelPoseChanged(XrPose pose) {
        assumeNonNull(mStrategy).onPlayerPanelPoseChanged(pose);
    }

    @Override
    public void onControlPanelPoseChanged(XrPose pose) {
        assumeNonNull(mStrategy).onControlPanelPoseChanged(pose);
    }

    @Override
    public void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {
        assumeNonNull(mStrategy).onPlayerPanelDragStart(origin, direction);
    }

    @Override
    public void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {
        assumeNonNull(mStrategy).onPlayerPanelDragUpdate(origin, direction);
    }

    @Override
    public void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {
        assumeNonNull(mStrategy).onPlayerPanelDragEnd(origin, direction);
    }

    @Override
    public XrPose getPlayerPanelPose() {
        return assumeNonNull(mStrategy).getPlayerPanelPose();
    }

    @Override
    public XrPose getControlPanelPose() {
        return assumeNonNull(mStrategy).getControlPanelPose();
    }
}
