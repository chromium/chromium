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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Tests for {@link XrFactoryImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrFactoryImplTest {
    private static final float DELTA = 1e-5f;

    @Mock private View mView;

    private Session mSession;
    private XrFactoryImpl mFactory;

    @Before
    public void setUp() {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        ComponentActivity activity =
                Robolectric.buildActivity(ComponentActivity.class).create().start().get();
        SessionCreateResult result = Session.create(activity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSession = ((SessionCreateSuccess) result).getSession();
        mFactory = new XrFactoryImpl();
    }

    @Test
    public void testCreateVector3() {
        XrVector3 vector = mFactory.createVector3(1.0f, 2.0f, 3.0f);
        assertNotNull(vector);
        assertEquals(1.0f, vector.getX(), DELTA);
        assertEquals(2.0f, vector.getY(), DELTA);
        assertEquals(3.0f, vector.getZ(), DELTA);
    }

    @Test
    public void testCreateQuaternion() {
        XrQuaternion quaternion = mFactory.createQuaternion(0.2f, 0.4f, 0.4f, 0.8f);
        assertNotNull(quaternion);
        assertEquals(0.2f, quaternion.getX(), DELTA);
        assertEquals(0.4f, quaternion.getY(), DELTA);
        assertEquals(0.4f, quaternion.getZ(), DELTA);
        assertEquals(0.8f, quaternion.getW(), DELTA);
    }

    @Test
    public void testCreatePose() {
        XrVector3 trans = mFactory.createVector3(1.0f, 2.0f, 3.0f);
        XrQuaternion rot = mFactory.createQuaternion(0.0f, 0.0f, 0.0f, 1.0f);
        XrPose pose = mFactory.createPose(trans, rot);
        assertNotNull(pose);
        assertEquals(trans, pose.getTranslation());
        assertEquals(rot, pose.getRotation());
    }

    @Test
    public void testCreateFloatSize3d() {
        XrFloatSize3d size = mFactory.createFloatSize3d(4.0f, 5.0f, 6.0f);
        assertNotNull(size);
        assertEquals(4.0f, size.getWidth(), DELTA);
        assertEquals(5.0f, size.getHeight(), DELTA);
        assertEquals(6.0f, size.getDepth(), DELTA);
    }

    @Test
    public void testCreatePanelEntity() {
        XrPanelEntityHolder holder = mFactory.createPanelEntity(mSession, mView, "testPanel");
        assertNotNull(holder);
    }

    @Test
    public void testCreateSurfaceEntity() {
        XrSurfaceEntityHolder holder =
                mFactory.createSurfaceEntity(mSession, XrSurfaceEntityShape.QUAD);
        assertNotNull(holder);
    }
}
