// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xr.scenecore.XrInteractableComponentImpl;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Tests for {@link XrCustomMeshHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressLint("RestrictedApiAndroidX")
public class XrCustomMeshHolderTest {
    private static final float DELTA = 0.01f;
    private static final float RADIUS = 2.5f;
    private static final int TEXTURE_WIDTH = 1920;
    private static final int TEXTURE_HEIGHT = 1080;

    private Session mSession;
    private SurfaceEntity mParentEntity;
    private XrInteractableComponentImpl<SurfaceEntity> mInteractableComponent;
    private XrSeamlessSphereMeshGenerator mSphereGenerator;
    private XrCurvedMeshHolder mSphereHolder;

    @Before
    public void setUp() {
        ComponentActivity activity =
                Robolectric.buildActivity(ComponentActivity.class).create().start().get();

        SessionCreateResult result = Session.create(activity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSession = ((SessionCreateSuccess) result).getSession();

        mParentEntity = SurfaceEntity.create(mSession);
        mInteractableComponent = new XrInteractableComponentImpl<>(mSession, mParentEntity);
    }

    private void createSphereHolder() {
        mSphereGenerator =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.MONO,
                                TEXTURE_WIDTH,
                                TEXTURE_HEIGHT,
                                RADIUS,
                                50));
        mSphereHolder =
                new XrCurvedMeshHolder(
                        mSession,
                        mParentEntity,
                        mInteractableComponent,
                        XrSurfaceEntityShape.SEAMLESS_SPHERE,
                        mSphereGenerator);
    }

    @Test
    public void testGetShape_Sphere() {
        createSphereHolder();
        assertEquals(XrSurfaceEntityShape.SEAMLESS_SPHERE, mSphereHolder.getShape());
    }

    @Test
    public void testGetGenerator_Sphere() {
        createSphereHolder();
        assertEquals(mSphereGenerator, mSphereHolder.getGenerator());
    }

    @Test
    public void testUpdateMesh_Sphere() {
        createSphereHolder();
        mSphereHolder.updateMesh();
        assertTrue(mParentEntity.getShape() instanceof Shape.CustomMesh);
        Shape.CustomMesh mesh = (Shape.CustomMesh) mParentEntity.getShape();
        assertNotNull(mesh.getLeftEye());
    }

    @Test
    public void testGetAndSetRadius() {
        createSphereHolder();
        assertEquals(RADIUS, mSphereHolder.getRadius(), DELTA);
        mSphereHolder.setRadius(5.0f);
        assertEquals(5.0f, mSphereHolder.getRadius(), DELTA);
        assertTrue(mParentEntity.getShape() instanceof Shape.CustomMesh);
    }

    @Test
    public void testSetStereoMode_Sphere() {
        createSphereHolder();
        mSphereHolder.setStereoMode(XrSurfaceEntityStereoMode.SIDE_BY_SIDE);
        Shape.CustomMesh sbsMesh = (Shape.CustomMesh) mParentEntity.getShape();
        assertNotNull(sbsMesh.getLeftEye());
        assertNotNull(sbsMesh.getRightEye());

        mSphereHolder.setStereoMode(XrSurfaceEntityStereoMode.MONO);
        Shape.CustomMesh monoMesh = (Shape.CustomMesh) mParentEntity.getShape();
        assertNotNull(monoMesh.getLeftEye());
        assertNull(monoMesh.getRightEye());
    }

    @Test
    public void testDispose() {
        createSphereHolder();
        mSphereHolder.dispose();
        assertTrue(mSphereHolder.isDisposed());
    }
}
