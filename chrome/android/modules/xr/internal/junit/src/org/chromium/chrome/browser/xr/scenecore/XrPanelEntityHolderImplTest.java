// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.util.Size;
import android.util.SizeF;
import android.view.View;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.runtime.math.Pose;
import androidx.xr.scenecore.PanelEntity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSpace;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Tests for {@link XrPanelEntityHolderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrPanelEntityHolderImplTest {
    private static final float DELTA = 0.01f;

    @Mock private View mView;

    private Session mSession;
    private XrPanelEntityHolderImpl mHolder;
    private ComponentActivity mActivity;
    private PanelEntity mPanelEntity;

    @Before
    public void setUp() {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);

        mActivity = Robolectric.buildActivity(ComponentActivity.class).create().start().get();

        SessionCreateResult result = Session.create(mActivity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSession = ((SessionCreateSuccess) result).getSession();

        mPanelEntity = PanelEntity.create(mSession, mView, new FloatSize2d(1f, 1f), "test-panel");
        mHolder = XrPanelEntityHolderImpl.create(mSession, mPanelEntity);
    }

    @Test
    public void testCreate() {
        assertNotNull(mHolder);
    }

    @Test
    public void testGetEntitySize() {
        mHolder.setEntitySize(10f, 20f);
        SizeF size = mHolder.getEntitySize();
        assertEquals(10f, size.getWidth(), DELTA);
        assertEquals(20f, size.getHeight(), DELTA);
    }

    @Test
    public void testSetEntitySize() {
        mHolder.setEntitySize(10f, 20f);
        assertEquals(10f, mPanelEntity.getSize().getWidth(), DELTA);
        assertEquals(20f, mPanelEntity.getSize().getHeight(), DELTA);
    }

    @Test
    public void testGetEntitySizeInPixels() {
        mHolder.setEntitySizeInPixels(100, 200);
        Size size = mHolder.getEntitySizeInPixels();
        assertEquals(100, size.getWidth());
        assertEquals(200, size.getHeight());
    }

    @Test
    public void testSetEntitySizeInPixels() {
        mHolder.setEntitySizeInPixels(100, 200);
        assertEquals(100, mPanelEntity.getSizeInPixels().getWidth());
        assertEquals(200, mPanelEntity.getSizeInPixels().getHeight());
    }

    @Test
    public void testGetEntityCornerRadius() {
        mHolder.setEntityCornerRadius(5f);
        assertEquals(5f, mHolder.getEntityCornerRadius(), DELTA);
    }

    @Test
    public void testSetEntityCornerRadius() {
        mHolder.setEntityCornerRadius(5f);
        assertEquals(5f, mPanelEntity.getCornerRadius(), DELTA);
    }

    @Test
    public void testSetEntityPose() {
        XrPose pose = XrPose.create(XrVector3.create(1f, 2f, 3f));
        mHolder.setEntityPose(pose, XrSpace.ACTIVITY);
        Pose actualPose = mPanelEntity.getPose(androidx.xr.scenecore.Space.ACTIVITY);
        assertEquals(1f, actualPose.getTranslation().getX(), DELTA);
    }

    @Test
    public void testGetEntityPose() {
        XrPose expectedPose = XrPose.create(XrVector3.create(1f, 2f, 3f));
        mHolder.setEntityPose(expectedPose, XrSpace.ACTIVITY);
        XrPose actualPose = mHolder.getEntityPose(XrSpace.ACTIVITY);
        assertEquals(1f, actualPose.getTranslation().getX(), DELTA);
        assertEquals(1f, actualPose.getRotation().getW(), DELTA);
    }

    @Test
    public void testGetEntityScale() {
        mHolder.setEntityScale(2.0f, XrSpace.ACTIVITY);
        assertEquals(2.0f, mHolder.getEntityScale(XrSpace.ACTIVITY), DELTA);
    }

    @Test
    public void testSetEntityScale() {
        mHolder.setEntityScale(2.0f, XrSpace.ACTIVITY);
        assertEquals(2.0f, mPanelEntity.getScale(androidx.xr.scenecore.Space.ACTIVITY), DELTA);
    }

    @Test
    public void testSetEntityAlpha() {
        mHolder.setEntityAlpha(0.5f);
        assertEquals(0.5f, mPanelEntity.getAlpha(androidx.xr.scenecore.Space.ACTIVITY), DELTA);
    }

    @Test
    public void testGetEntityAlpha() {
        mHolder.setEntityAlpha(0.5f);
        assertEquals(0.5f, mHolder.getEntityAlpha(XrSpace.ACTIVITY), DELTA);
    }

    @Test
    public void testSetEntityEnabled() {
        mHolder.setEntityEnabled(true);
        assertTrue(mPanelEntity.isEnabled(true));
    }

    @Test
    public void testIsEntityEnabled() {
        mHolder.setEntityEnabled(true);
        assertTrue(mHolder.isEntityEnabled());
    }

    @Test
    public void testGetInteractableComponent() {
        assertNotNull(mHolder.getInteractableComponent());
    }

    @Test
    public void testDispose() {
        mHolder.dispose();
        assertTrue(mPanelEntity.isDisposed());
    }
}
