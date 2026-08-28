// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import android.util.SizeF;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.xr.scenecore.XrInteractableComponentImpl;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;

/**
 * Helper class managing a {@link SurfaceEntity} with dynamic 2D planar {@link
 * XrPlanarMeshGenerator} geometry.
 */
@NullMarked
public class XrPlanarMeshHolder extends XrCustomMeshHolder<XrPlanarMeshGenerator> {

    public XrPlanarMeshHolder(
            Session xrSession,
            SurfaceEntity parentEntity,
            XrInteractableComponentImpl interactableComponent,
            @XrSurfaceEntityShape int shape,
            XrPlanarMeshGenerator generator) {
        super(xrSession, parentEntity, interactableComponent, shape, generator);

        // Reset SceneCore internal entity node scale by temporarily setting a uniform shape (Quad),
        // because SceneCore does not reset local scale when setting Shape.CustomMesh.
        mParentEntity.setShape(new Shape.Quad(new FloatSize2d(1f, 1f)));
    }

    public SizeF getSize() {
        assertDisposed();
        return new SizeF(mCustomMeshGenerator.getWidth(), mCustomMeshGenerator.getHeight());
    }

    public float getWidth() {
        assertDisposed();
        return mCustomMeshGenerator.getWidth();
    }

    public float getHeight() {
        assertDisposed();
        return mCustomMeshGenerator.getHeight();
    }

    public float getCornerRadius() {
        assertDisposed();
        return mCustomMeshGenerator.getCornerRadius();
    }

    public void setCornerRadius(float cornerRadius) {
        assertDisposed();
        mCustomMeshGenerator.setCornerRadius(cornerRadius);
        updateMesh();
    }

    public void setDimensions(float width, float height) {
        assertDisposed();
        mCustomMeshGenerator.setWidth(width);
        mCustomMeshGenerator.setHeight(height);
        updateCollider();
        updateMesh();
    }
}
