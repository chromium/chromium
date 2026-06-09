// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ImmersiveProjectionType;

/** Helper class that manages 3D spatial coordinate calculations and center screen tracking. */
@NullMarked
public class ImmersiveVideoPoseManager {
    private static final float[] IDENTITY_ROTATION = new float[] {0f, 0f, 0f, 1f};
    private static final float[] ORIGIN_TRANSLATION = new float[] {0f, 0f, 0f};

    /** Delegate for providing layout dimensions needed for vertical offset calculations. */
    public interface Delegate {
        /** Returns the layout height of the video surface. */
        float getLayoutHeight();
    }

    private float[] mCenterTranslation = new float[] {0f, 0f, 0.5f};
    private float[] mCenterRotation = new float[] {0f, 0f, 0f, 1f};
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
    public void onPlayerPanelPoseChanged(
            float[] translation, float[] rotation, @ImmersiveProjectionType int projectionType) {
        if (projectionType == ImmersiveProjectionType.QUAD) {
            mCenterTranslation = translation.clone();
            mCenterRotation = rotation.clone();
        }
    }

    /** Called when the pose of the control panel changes during interaction. */
    public void onControlPanelPoseChanged(
            float[] translation, float[] rotation, @ImmersiveProjectionType int projectionType) {
        if (projectionType != ImmersiveProjectionType.QUAD) {
            float[] playerTranslation = translation.clone();
            playerTranslation[1] -= getVerticalOffset();
            mCenterTranslation = playerTranslation;
            mCenterRotation = rotation.clone();
        }
    }

    /**
     * Returns the expected translation for the player panel based on the current projection mode.
     */
    public float[] getPlayerPanelTranslation(@ImmersiveProjectionType int projectionType) {
        return projectionType == ImmersiveProjectionType.QUAD
                ? mCenterTranslation.clone()
                : ORIGIN_TRANSLATION;
    }

    /** Returns the expected rotation for the player panel based on the current projection mode. */
    public float[] getPlayerPanelRotation(@ImmersiveProjectionType int projectionType) {
        return projectionType == ImmersiveProjectionType.QUAD
                ? mCenterRotation.clone()
                : IDENTITY_ROTATION;
    }

    /**
     * Returns the expected translation for the control panel based on projection and vertical
     * offset.
     */
    public float[] getControlPanelTranslation(@ImmersiveProjectionType int projectionType) {
        float verticalOffset = getVerticalOffset();
        if (projectionType == ImmersiveProjectionType.QUAD) {
            return new float[] {0f, verticalOffset, 0f};
        } else {
            return new float[] {
                mCenterTranslation[0], mCenterTranslation[1] + verticalOffset, mCenterTranslation[2]
            };
        }
    }

    /** Returns the expected rotation for the control panel based on the current projection mode. */
    public float[] getControlPanelRotation(@ImmersiveProjectionType int projectionType) {
        return projectionType == ImmersiveProjectionType.QUAD
                ? IDENTITY_ROTATION
                : mCenterRotation.clone();
    }

    private float getVerticalOffset() {
        return -mDelegate.getLayoutHeight() / 2f;
    }
}
