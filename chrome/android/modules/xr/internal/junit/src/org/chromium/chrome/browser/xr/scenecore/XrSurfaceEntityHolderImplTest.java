// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;
import androidx.xr.scenecore.SurfaceEntity.StereoMode;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder.Callback;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Tests for {@link XrSurfaceEntityHolderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrSurfaceEntityHolderImplTest {
    private static final float DELTA = 0.01f;

    @Mock private Callback mCallback;

    private Session mSession;
    private XrSurfaceEntityHolderImpl mHolder;
    private ComponentActivity mActivity;
    private SurfaceEntity mSurfaceEntity;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mActivity = Robolectric.buildActivity(ComponentActivity.class).create().start().get();

        SessionCreateResult result = Session.create(mActivity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSession = ((SessionCreateSuccess) result).getSession();

        mSurfaceEntity = SurfaceEntity.create(mSession);
        mHolder = XrSurfaceEntityHolderImpl.create(mSession, mSurfaceEntity);
    }

    @Test
    public void testCreate() {
        assertNotNull(mHolder);
    }

    @Test
    public void testDispose() {
        mHolder.dispose();
        assertTrue(mSurfaceEntity.isDisposed());
    }

    @Test
    public void testAddCallback() {
        mHolder.addCallback(mCallback);
        assertTrue(mHolder.hasCallbackForTesting(mCallback));
    }

    @Test
    public void testRemoveCallback() {
        mHolder.addCallback(mCallback);
        assertTrue(mHolder.hasCallbackForTesting(mCallback));
        mHolder.removeCallback(mCallback);
        assertFalse(mHolder.hasCallbackForTesting(mCallback));
    }

    @Test
    public void testSetSurfacePixelDimensions() {
        mHolder.addCallback(mCallback);
        mHolder.setSurfacePixelDimensions(100, 200);
        verify(mCallback).surfaceChanged(any(), eq(100), eq(200));
    }

    @Test
    public void testGetSurfaceStereoMode() {
        mHolder.setSurfaceStereoMode(XrSurfaceEntityStereoMode.MONO);
        assertEquals(XrSurfaceEntityStereoMode.MONO, mHolder.getSurfaceStereoMode());
    }

    @Test
    public void testSetSurfaceStereoMode() {
        mHolder.setSurfaceStereoMode(XrSurfaceEntityStereoMode.SIDE_BY_SIDE);
        assertEquals(StereoMode.SIDE_BY_SIDE, mSurfaceEntity.getStereoMode());
        mHolder.setSurfaceStereoMode(XrSurfaceEntityStereoMode.TOP_BOTTOM);
        assertEquals(StereoMode.TOP_BOTTOM, mSurfaceEntity.getStereoMode());
        mHolder.setSurfaceStereoMode(XrSurfaceEntityStereoMode.MONO);
        assertEquals(StereoMode.MONO, mSurfaceEntity.getStereoMode());
    }

    @Test
    public void testGetSurfaceShape() {
        mHolder.setSurfaceShape(XrSurfaceEntityShape.QUAD);
        assertEquals(XrSurfaceEntityShape.QUAD, mHolder.getSurfaceShape());
    }

    @Test
    public void testSetSurfaceShape() {
        mHolder.setSurfaceShape(XrSurfaceEntityShape.SPHERE);
        assertEquals(XrSurfaceEntityShape.SPHERE, mHolder.getSurfaceShape());
        mHolder.setSurfaceShape(XrSurfaceEntityShape.HEMISPHERE);
        assertEquals(XrSurfaceEntityShape.HEMISPHERE, mHolder.getSurfaceShape());
        mHolder.setSurfaceShape(XrSurfaceEntityShape.QUAD);
        assertEquals(XrSurfaceEntityShape.QUAD, mHolder.getSurfaceShape());
    }

    @Test
    public void testSetEntitySize_Quad() {
        mHolder.setSurfaceShape(XrSurfaceEntityShape.QUAD);
        mHolder.setEntitySize(10f, 20f);

        Shape shape = mSurfaceEntity.getShape();
        assertTrue(shape instanceof Shape.Quad);
        assertEquals(10f, ((Shape.Quad) shape).getExtents().getWidth(), DELTA);
    }

    @Test
    public void testSetEntityRadius_Sphere() {
        mHolder.setSurfaceShape(XrSurfaceEntityShape.SPHERE);
        mHolder.setEntityRadius(5f);

        Shape shape = mSurfaceEntity.getShape();
        assertTrue(shape instanceof Shape.Sphere);
        assertEquals(5f, ((Shape.Sphere) shape).getRadius(), DELTA);
    }
}
