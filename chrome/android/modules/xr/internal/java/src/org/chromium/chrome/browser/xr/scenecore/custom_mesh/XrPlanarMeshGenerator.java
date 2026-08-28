// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import android.annotation.SuppressLint;

import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/**
 * Abstract base class for generating 2D / planar custom mesh shapes for SceneCore surface entities.
 */
@NullMarked
@SuppressLint("RestrictedApiAndroidX")
public abstract class XrPlanarMeshGenerator
        extends XrCustomMeshGenerator<XrPlanarMeshGenerator.Config> {

    /** Configuration parameters for generating a planar custom mesh. */
    public static class Config extends XrCustomMeshGenerator.Config {
        public static final float DEFAULT_WIDTH = 1.0f;
        public static final float DEFAULT_HEIGHT = 1.0f;
        public static final float DEFAULT_CORNER_RADIUS = 0.0f;
        public static final int MIN_CORNER_RESOLUTION = 1;
        public static final int DEFAULT_CORNER_RESOLUTION = 10;

        private float mWidth = DEFAULT_WIDTH;
        private float mHeight = DEFAULT_HEIGHT;
        private float mCornerRadius = DEFAULT_CORNER_RADIUS;
        private int mCornerResolution = DEFAULT_CORNER_RESOLUTION;

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode,
                int textureWidth,
                int textureHeight,
                float width,
                float height,
                float cornerRadius,
                int cornerResolution,
                boolean flipUv) {
            super(stereoMode, textureWidth, textureHeight, flipUv);
            mWidth = Math.max(0f, width);
            mHeight = Math.max(0f, height);
            mCornerRadius = Math.max(0f, cornerRadius);
            mCornerResolution = Math.max(MIN_CORNER_RESOLUTION, cornerResolution);
        }

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode,
                int textureWidth,
                int textureHeight,
                float width,
                float height,
                float cornerRadius,
                int cornerResolution) {
            this(
                    stereoMode,
                    textureWidth,
                    textureHeight,
                    width,
                    height,
                    cornerRadius,
                    cornerResolution,
                    DEFAULT_FLIP_UV);
        }

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode, int textureWidth, int textureHeight) {
            this(
                    stereoMode,
                    textureWidth,
                    textureHeight,
                    DEFAULT_WIDTH,
                    DEFAULT_HEIGHT,
                    DEFAULT_CORNER_RADIUS,
                    DEFAULT_CORNER_RESOLUTION,
                    DEFAULT_FLIP_UV);
        }

        public float getWidth() {
            return mWidth;
        }

        public void setWidth(float width) {
            mWidth = Math.max(0f, width);
        }

        public float getHeight() {
            return mHeight;
        }

        public void setHeight(float height) {
            mHeight = Math.max(0f, height);
        }

        public float getCornerRadius() {
            return mCornerRadius;
        }

        public void setCornerRadius(float cornerRadius) {
            mCornerRadius = Math.max(0f, cornerRadius);
        }

        public int getCornerResolution() {
            return mCornerResolution;
        }

        public void setCornerResolution(int cornerResolution) {
            mCornerResolution = Math.max(MIN_CORNER_RESOLUTION, cornerResolution);
        }
    }

    public XrPlanarMeshGenerator(Config config) {
        super(config);
    }

    /** Returns the width of the planar mesh shape. */
    public float getWidth() {
        return mConfig.getWidth();
    }

    /** Sets the width of the planar mesh shape. */
    public void setWidth(float width) {
        mConfig.setWidth(width);
    }

    /** Returns the height of the planar mesh shape. */
    public float getHeight() {
        return mConfig.getHeight();
    }

    /** Sets the height of the planar mesh shape. */
    public void setHeight(float height) {
        mConfig.setHeight(height);
    }

    /** Returns the corner radius of the planar mesh shape. */
    public float getCornerRadius() {
        return mConfig.getCornerRadius();
    }

    /** Sets the corner radius of the planar mesh shape. */
    public void setCornerRadius(float cornerRadius) {
        mConfig.setCornerRadius(cornerRadius);
    }

    /** Returns the corner resolution (segments per corner arc). */
    public int getCornerResolution() {
        return mConfig.getCornerResolution();
    }

    /** Sets the corner resolution (segments per corner arc). */
    public void setCornerResolution(int cornerResolution) {
        mConfig.setCornerResolution(cornerResolution);
    }

    @Override
    public Shape createColliderShape() {
        return new Shape.Quad(new FloatSize2d(getWidth(), getHeight()));
    }
}
