// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import android.annotation.SuppressLint;

import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/**
 * Abstract base class for generating 3D curved custom mesh shapes for SceneCore surface entities.
 */
@NullMarked
@SuppressLint("RestrictedApiAndroidX")
public abstract class XrCurvedMeshGenerator
        extends XrCustomMeshGenerator<XrCurvedMeshGenerator.Config> {

    /** Configuration parameters for generating a curved custom mesh. */
    public static class Config extends XrCustomMeshGenerator.Config {
        public static final float DEFAULT_RADIUS = 1.0f;
        public static final float DEFAULT_PADDING_TEXELS = 0.25f;
        public static final int MIN_RESOLUTION = 8;
        public static final int DEFAULT_RESOLUTION = 50;

        private float mRadius = DEFAULT_RADIUS;
        private float mPaddingTexels = DEFAULT_PADDING_TEXELS;
        private int mResolution = DEFAULT_RESOLUTION;

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode,
                int textureWidth,
                int textureHeight,
                float radius,
                int resolution,
                boolean flipUv,
                float paddingTexels) {
            super(stereoMode, textureWidth, textureHeight, flipUv);
            mRadius = Math.max(0f, radius);
            mResolution = Math.max(MIN_RESOLUTION, resolution);
            mPaddingTexels = Math.max(0f, paddingTexels);
        }

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode,
                int textureWidth,
                int textureHeight,
                float radius,
                int resolution) {
            this(
                    stereoMode,
                    textureWidth,
                    textureHeight,
                    radius,
                    resolution,
                    DEFAULT_FLIP_UV,
                    DEFAULT_PADDING_TEXELS);
        }

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode, int textureWidth, int textureHeight) {
            this(
                    stereoMode,
                    textureWidth,
                    textureHeight,
                    DEFAULT_RADIUS,
                    DEFAULT_RESOLUTION,
                    DEFAULT_FLIP_UV,
                    DEFAULT_PADDING_TEXELS);
        }

        public float getRadius() {
            return mRadius;
        }

        public void setRadius(float radius) {
            mRadius = Math.max(0f, radius);
        }

        public float getPaddingTexels() {
            return mPaddingTexels;
        }

        public void setPaddingTexels(float paddingTexels) {
            mPaddingTexels = Math.max(0f, paddingTexels);
        }

        public int getResolution() {
            return mResolution;
        }

        public void setResolution(int resolution) {
            mResolution = Math.max(MIN_RESOLUTION, resolution);
        }
    }

    public XrCurvedMeshGenerator(Config config) {
        super(config);
    }

    /** Sets the radius for the curved mesh shape. */
    public void setRadius(float radius) {
        mConfig.setRadius(radius);
    }

    /** Returns the radius of the curved mesh shape. */
    public float getRadius() {
        return mConfig.getRadius();
    }

    /** Sets the texture coordinate padding in texels. */
    public void setPaddingTexels(float paddingTexels) {
        mConfig.setPaddingTexels(paddingTexels);
    }

    /** Returns the texture coordinate padding in texels. */
    public float getPaddingTexels() {
        return mConfig.getPaddingTexels();
    }

    /** Returns the resolution (latitude/longitude subdivisions). */
    public int getResolution() {
        return mConfig.getResolution();
    }

    /** Sets the resolution (latitude/longitude subdivisions). */
    public void setResolution(int resolution) {
        mConfig.setResolution(resolution);
    }

    @Override
    public Shape createColliderShape() {
        return new Shape.Sphere(getRadius());
    }
}
