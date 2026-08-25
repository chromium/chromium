// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import androidx.xr.runtime.Session;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.xr.scenecore.XrInteractableComponentImpl;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;

/**
 * Helper class managing a {@link SurfaceEntity} with dynamic 3D curved {@link
 * XrCurvedMeshGenerator} geometry.
 */
@NullMarked
public class XrCurvedMeshHolder extends XrCustomMeshHolder<XrCurvedMeshGenerator> {

    public XrCurvedMeshHolder(
            Session xrSession,
            SurfaceEntity parentEntity,
            XrInteractableComponentImpl interactableComponent,
            @XrSurfaceEntityShape int shape,
            XrCurvedMeshGenerator generator) {
        super(xrSession, parentEntity, interactableComponent, shape, generator);

        // Reset SceneCore internal entity node scale by temporarily setting a uniform shape
        // (Sphere), because SceneCore does not reset local scale when setting Shape.CustomMesh.
        mParentEntity.setShape(new Shape.Sphere(1f));
    }

    public float getRadius() {
        assertDisposed();
        return mCustomMeshGenerator.getRadius();
    }

    public void setRadius(float radius) {
        assertDisposed();
        mCustomMeshGenerator.setRadius(radius);
        updateCollider();
        updateMesh();
    }
}
