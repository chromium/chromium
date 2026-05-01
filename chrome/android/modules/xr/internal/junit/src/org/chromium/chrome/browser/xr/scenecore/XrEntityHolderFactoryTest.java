// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.scenecore.PanelEntity;
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
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrQuadSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;

/** Tests for {@link XrEntityHolderFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrEntityHolderFactoryTest {

    @Mock private View mView;

    private Session mSession;
    private ComponentActivity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mActivity = Robolectric.buildActivity(ComponentActivity.class).create().start().get();

        SessionCreateResult result = Session.create(mActivity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSession = ((SessionCreateSuccess) result).getSession();
    }

    @Test
    public void testCreateSurfaceEntityHolder_Quad() {
        XrSurfaceEntityHolder holder =
                XrEntityHolderFactory.createSurfaceEntityHolder(
                        mSession, XrSurfaceEntityShape.QUAD);
        assertNotNull(holder);
        assertTrue(holder instanceof XrQuadSurfaceEntityHolder);

        SurfaceEntity surfaceEntity = (SurfaceEntity) holder.getEntity();
        assertEquals(StereoMode.MONO, surfaceEntity.getStereoMode());
        assertTrue(surfaceEntity.getShape() instanceof Shape.Quad);
    }

    @Test
    public void testCreateSurfaceEntityHolder_Sphere() {
        XrSurfaceEntityHolder holder =
                XrEntityHolderFactory.createSurfaceEntityHolder(
                        mSession, XrSurfaceEntityShape.SPHERE);
        assertNotNull(holder);
        assertTrue(holder instanceof XrCurvedSurfaceEntityHolder);

        SurfaceEntity surfaceEntity = (SurfaceEntity) holder.getEntity();
        assertEquals(StereoMode.MONO, surfaceEntity.getStereoMode());
        assertTrue(surfaceEntity.getShape() instanceof Shape.Sphere);
    }

    @Test
    public void testCreateSurfaceEntityHolder_Hemisphere() {
        XrSurfaceEntityHolder holder =
                XrEntityHolderFactory.createSurfaceEntityHolder(
                        mSession, XrSurfaceEntityShape.HEMISPHERE);
        assertNotNull(holder);
        assertTrue(holder instanceof XrCurvedSurfaceEntityHolder);

        SurfaceEntity surfaceEntity = (SurfaceEntity) holder.getEntity();
        assertEquals(StereoMode.MONO, surfaceEntity.getStereoMode());
        assertTrue(surfaceEntity.getShape() instanceof Shape.Hemisphere);
    }

    @Test
    public void testCreatePanelEntityHolder() {
        XrPanelEntityHolder holder =
                XrEntityHolderFactory.createPanelEntityHolder(mSession, mView, "test-panel");
        assertNotNull(holder);

        PanelEntity panelEntity = (PanelEntity) holder.getEntity();
        assertNotNull(panelEntity);
    }
}
