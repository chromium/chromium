// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import android.annotation.SuppressLint;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.view.Surface;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.IntSize2d;
import androidx.xr.runtime.math.Pose;
import androidx.xr.scenecore.InteractableComponent;
import androidx.xr.scenecore.Space;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;
import androidx.xr.scenecore.SurfaceEntity.StereoMode;
import androidx.xr.scenecore.SurfaceEntity.SuperSampling;
import androidx.xr.scenecore.SurfaceEntity.SurfaceProtection;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.xr.scenecore.XrInteractableComponentImpl;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/**
 * Abstract base class managing a {@link SurfaceEntity} with dynamic {@link Shape.CustomMesh}
 * geometry and its associated collider and interaction state.
 */
@NullMarked
@SuppressLint("RestrictedApiAndroidX")
public abstract class XrCustomMeshHolder<GeneratorType extends XrCustomMeshGenerator> {
    private static final String TAG = "XrCustomMeshHolder";

    protected final Session mXrSession;
    protected final SurfaceEntity mParentEntity;
    protected final XrInteractableComponentImpl mInteractableComponent;
    protected final @XrSurfaceEntityShape int mShape;
    protected final GeneratorType mCustomMeshGenerator;
    protected final SurfaceEntity mColliderEntity;
    private boolean mIsDisposed;

    public XrCustomMeshHolder(
            Session xrSession,
            SurfaceEntity parentEntity,
            XrInteractableComponentImpl interactableComponent,
            @XrSurfaceEntityShape int shape,
            GeneratorType generator) {
        mXrSession = xrSession;
        mParentEntity = parentEntity;
        mInteractableComponent = interactableComponent;
        mShape = shape;
        mCustomMeshGenerator = generator;
        mColliderEntity =
                createColliderEntity(xrSession, parentEntity, generator.createColliderShape());
        mColliderEntity.addComponent(
                InteractableComponent.create(mXrSession, mInteractableComponent::onInputEvent));
        mInteractableComponent.setChildColliderEntity(mColliderEntity);
    }

    protected void assertDisposed() {
        if (mIsDisposed) {
            throw new IllegalStateException("Custom mesh holder is already disposed");
        }
    }

    public @XrSurfaceEntityShape int getShape() {
        assertDisposed();
        return mShape;
    }

    public GeneratorType getGenerator() {
        assertDisposed();
        return mCustomMeshGenerator;
    }

    public void updateMesh() {
        assertDisposed();
        mParentEntity.setShape(mCustomMeshGenerator.toCustomMesh());
    }

    public void updateCollider() {
        assertDisposed();
        mColliderEntity.setShape(mCustomMeshGenerator.createColliderShape());
    }

    public void setStereoMode(@XrSurfaceEntityStereoMode int stereoMode) {
        assertDisposed();
        mCustomMeshGenerator.setStereoMode(stereoMode);
        updateMesh();
    }

    public void setSurfacePixelDimensions(int width, int height) {
        assertDisposed();
        mCustomMeshGenerator.setSurfacePixelDimensions(width, height);
        updateMesh();
    }

    public void dispose() {
        if (!mIsDisposed) {
            mInteractableComponent.setChildColliderEntity(null);
            mColliderEntity.dispose();
            mIsDisposed = true;
        }
    }

    public boolean isDisposed() {
        return mIsDisposed;
    }

    /**
     * Creates a collider entity that matches the size of the custom mesh. This is a workaround for
     * SceneCore not supporting custom mesh colliders.
     *
     * <p>TODO(crbug.com/487480373): Remove once SceneCore implements proper collider support.
     *
     * @param xrSession The {@link Session} to create the collider entity in.
     * @param parentEntity The parent entity of the collider entity.
     * @param colliderShape The shape of the collider entity.
     * @return The collider entity.
     */
    private static SurfaceEntity createColliderEntity(
            Session xrSession, SurfaceEntity parentEntity, Shape colliderShape) {
        SurfaceEntity colliderEntity =
                SurfaceEntity.create(
                        xrSession,
                        Pose.Identity,
                        colliderShape,
                        StereoMode.MONO,
                        SuperSampling.PENTAGON,
                        SurfaceProtection.NONE);
        colliderEntity.setParent(parentEntity);
        colliderEntity.setPose(Pose.Identity, Space.PARENT);
        colliderEntity.setAlpha(0f);
        colliderEntity.setSurfacePixelDimensions(new IntSize2d(1, 1));
        clearSurfaceToTransparent(colliderEntity.getSurface());
        return colliderEntity;
    }

    /**
     * Clears the collider surface by submitting a 1x1 transparent frame.
     *
     * <p>When a child {@link SurfaceEntity} is parented to another {@link SurfaceEntity}, SceneCore
     * inherits and renders the parent's surface texture onto the child if the child has no frame
     * submitted to its own swapchain. Submitting a transparent buffer ensures the collider entity
     * remains completely invisible.
     *
     * @param surface The {@link Surface} to clear.
     */
    private static void clearSurfaceToTransparent(@Nullable Surface surface) {
        if (surface != null && surface.isValid()) {
            try {
                Canvas canvas = surface.lockCanvas(null);
                if (canvas != null) {
                    canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
                    surface.unlockCanvasAndPost(canvas);
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to clear surface to transparent", e);
            }
        }
    }
}
