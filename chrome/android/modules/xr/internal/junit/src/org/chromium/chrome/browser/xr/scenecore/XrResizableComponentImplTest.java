// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.scenecore.Component;
import androidx.xr.scenecore.PanelEntity;
import androidx.xr.scenecore.ResizableComponent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrResizableComponent.OnResizeListener;

import java.util.List;

/** Tests for {@link XrResizableComponentImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrResizableComponentImplTest {
    private static final float DELTA = 0.01f;

    @Mock private View mView;
    @Mock private OnResizeListener mListener;

    private Session mSession;
    private XrResizableComponentImpl<PanelEntity> mResizableComponent;
    private ComponentActivity mActivity;
    private PanelEntity mEntity;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mActivity = Robolectric.buildActivity(ComponentActivity.class).create().start().get();

        SessionCreateResult result = Session.create(mActivity);
        assertTrue(result instanceof SessionCreateSuccess);
        mSession = ((SessionCreateSuccess) result).getSession();

        mEntity = PanelEntity.create(mSession, mView, new FloatSize2d(1f, 1f), "test-panel");
        mResizableComponent = new XrResizableComponentImpl<>(mSession, mEntity);
    }

    private ResizableComponent getResizableComponent() {
        List<Component> components = mEntity.getComponents();
        for (Component c : components) {
            if (c instanceof ResizableComponent) {
                return (ResizableComponent) c;
            }
        }
        return null;
    }

    @Test
    public void testCreate() {
        assertNotNull(mResizableComponent);
    }

    @Test
    public void testDispose() {
        mResizableComponent.setResizable(true, false);
        mResizableComponent.addResizeListener(mListener);
        assertTrue(mResizableComponent.hasResizeListenerForTesting(mListener));

        mResizableComponent.dispose();

        assertNull(getResizableComponent());
        assertFalse(mResizableComponent.hasResizeListenerForTesting(mListener));
    }

    @Test
    public void testSetMinSize() {
        mResizableComponent.setMinSize(1f, 2f);
        mResizableComponent.setResizable(true, false);

        ResizableComponent resizableComponent = getResizableComponent();
        assertNotNull(resizableComponent);
        assertEquals(1f, resizableComponent.getMinimumEntitySize().getWidth(), DELTA);
        assertEquals(2f, resizableComponent.getMinimumEntitySize().getHeight(), DELTA);
    }

    @Test
    public void testSetMaxSize() {
        mResizableComponent.setMaxSize(10f, 20f);
        mResizableComponent.setResizable(true, false);

        ResizableComponent resizableComponent = getResizableComponent();
        assertNotNull(resizableComponent);
        assertEquals(10f, resizableComponent.getMaximumEntitySize().getWidth(), DELTA);
        assertEquals(20f, resizableComponent.getMaximumEntitySize().getHeight(), DELTA);
    }

    @Test
    public void testSetResizable_True() {
        mResizableComponent.setResizable(true, true);

        ResizableComponent resizableComponent = getResizableComponent();
        assertNotNull(resizableComponent);
        assertTrue(resizableComponent.isFixedAspectRatioEnabled());
        assertEquals(1, mEntity.getComponents().size());
    }

    @Test
    public void testSetResizable_False() {
        mResizableComponent.setResizable(true, false);
        assertEquals(1, mEntity.getComponents().size());

        mResizableComponent.setResizable(false, false);
        assertEquals(0, mEntity.getComponents().size());
    }

    @Test
    public void testSetResizable_Twice() {
        mResizableComponent.setResizable(true, false);
        ResizableComponent firstComponent = getResizableComponent();
        assertNotNull(firstComponent);

        mResizableComponent.setResizable(true, true);
        ResizableComponent secondComponent = getResizableComponent();
        assertNotNull(secondComponent);

        assertFalse(firstComponent == secondComponent);
        assertEquals(1, mEntity.getComponents().size());
    }

    @Test
    public void testAddResizeListener() {
        mResizableComponent.setResizable(true, false);
        mResizableComponent.addResizeListener(mListener);

        ResizableComponent resizable = getResizableComponent();
        assertNotNull(resizable);
    }

    @Test
    public void testAddResizeListener_Twice() {
        mResizableComponent.setResizable(true, false);
        mResizableComponent.addResizeListener(mListener);
        mResizableComponent.addResizeListener(mListener);

        assertTrue(mResizableComponent.hasResizeListenerForTesting(mListener));

        mResizableComponent.removeResizeListener(mListener);
        assertFalse(mResizableComponent.hasResizeListenerForTesting(mListener));
    }

    @Test
    public void testRemoveResizeListener() {
        mResizableComponent.setResizable(true, false);
        mResizableComponent.addResizeListener(mListener);
        assertTrue(mResizableComponent.hasResizeListenerForTesting(mListener));

        mResizableComponent.removeResizeListener(mListener);

        assertFalse(mResizableComponent.hasResizeListenerForTesting(mListener));
    }
}
