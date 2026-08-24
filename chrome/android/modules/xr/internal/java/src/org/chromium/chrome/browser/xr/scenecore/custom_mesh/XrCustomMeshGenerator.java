// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import android.annotation.SuppressLint;

import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.xr.scenecore.XrSurfaceEntityUtils;
import org.chromium.ui.xr.scenecore.XrMeshData;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Abstract base class for generating dynamic custom mesh shapes for SceneCore surface entities. */
@NullMarked
@SuppressLint("RestrictedApiAndroidX")
public abstract class XrCustomMeshGenerator<ConfigType extends XrCustomMeshGenerator.Config> {

    /** Base configuration parameters for custom mesh generation. */
    public static class Config {
        /**
         * Default value for whether to vertically flip UV (texture mapping) coordinates. UV
         * coordinates map 2D texture space (U = horizontal, V = vertical) to 3D mesh vertices.
         */
        public static final boolean DEFAULT_FLIP_UV = true;

        private @XrSurfaceEntityStereoMode int mStereoMode;
        private int mTextureWidth;
        private int mTextureHeight;

        /** Whether to vertically flip UV texture coordinates (inverting the V axis). */
        private boolean mFlipUv;

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode,
                int textureWidth,
                int textureHeight,
                boolean flipUv) {
            mStereoMode = stereoMode;
            mTextureWidth = Math.max(1, textureWidth);
            mTextureHeight = Math.max(1, textureHeight);
            mFlipUv = flipUv;
        }

        public Config(
                @XrSurfaceEntityStereoMode int stereoMode, int textureWidth, int textureHeight) {
            this(stereoMode, textureWidth, textureHeight, DEFAULT_FLIP_UV);
        }

        /** Returns the stereo mode used for custom mesh generation. */
        public @XrSurfaceEntityStereoMode int getStereoMode() {
            return mStereoMode;
        }

        /** Sets the stereo mode used for custom mesh generation. */
        public void setStereoMode(@XrSurfaceEntityStereoMode int stereoMode) {
            mStereoMode = stereoMode;
        }

        /** Returns whether UV (texture mapping) coordinates should be vertically flipped. */
        public boolean getFlipUv() {
            return mFlipUv;
        }

        /** Sets whether UV (texture mapping) coordinates should be vertically flipped. */
        public void setFlipUv(boolean flipUv) {
            mFlipUv = flipUv;
        }

        /** Returns the width of the texture in pixels. */
        public int getTextureWidth() {
            return mTextureWidth;
        }

        /** Returns the height of the texture in pixels. */
        public int getTextureHeight() {
            return mTextureHeight;
        }

        /** Returns the effective width of a single eye view within the texture. */
        public int getEyeWidth() {
            return mStereoMode == XrSurfaceEntityStereoMode.SIDE_BY_SIDE
                    ? Math.max(1, mTextureWidth / 2)
                    : mTextureWidth;
        }

        /** Returns the effective height of a single eye view within the texture. */
        public int getEyeHeight() {
            return mStereoMode == XrSurfaceEntityStereoMode.TOP_BOTTOM
                    ? Math.max(1, mTextureHeight / 2)
                    : mTextureHeight;
        }

        /** Sets the surface pixel dimensions used for custom mesh generation. */
        public void setSurfacePixelDimensions(int width, int height) {
            mTextureWidth = Math.max(1, width);
            mTextureHeight = Math.max(1, height);
        }
    }

    protected final ConfigType mConfig;

    public XrCustomMeshGenerator(ConfigType config) {
        mConfig = config;
    }

    /** Returns the configuration object for this mesh generator. */
    public ConfigType getConfig() {
        return mConfig;
    }

    /** Sets the stereo mode used for custom mesh generation. */
    public void setStereoMode(@XrSurfaceEntityStereoMode int stereoMode) {
        mConfig.setStereoMode(stereoMode);
    }

    /** Sets the surface pixel dimensions used for custom mesh generation. */
    public void setSurfacePixelDimensions(int width, int height) {
        mConfig.setSurfacePixelDimensions(width, height);
    }

    /** Generates the {@link XrMeshData} geometry for a single eye mesh. */
    protected abstract XrMeshData generateMeshData();

    /** Creates the interaction {@link Shape} collider corresponding to this mesh geometry. */
    public abstract Shape createColliderShape();

    /** Creates a {@link Shape.CustomMesh} instance from the current configuration. */
    public Shape.CustomMesh toCustomMesh() {
        XrMeshData mesh = generateMeshData();
        XrMeshData[] meshes =
                mConfig.getStereoMode() == XrSurfaceEntityStereoMode.MONO
                        ? new XrMeshData[] {mesh}
                        : new XrMeshData[] {mesh, mesh};
        Shape.CustomMesh customMesh = XrSurfaceEntityUtils.createCustomMesh(meshes);
        assert customMesh != null;
        return customMesh;
    }
}
