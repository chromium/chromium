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
import androidx.xr.scenecore.MovableComponent;
import androidx.xr.scenecore.PanelEntity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrMovableComponent.OnMoveListener;

import java.util.List;

/** Tests for {@link XrMovableComponentImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrMovableComponentImplTest {

    @Mock private OnMoveListener mListener;
    @Mock private View mView;

    private Session mSession;
    private XrMovableComponentImpl<PanelEntity> mMovableComponent;
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
        mMovableComponent = new XrMovableComponentImpl<>(mSession, mEntity);
    }

    private MovableComponent getMovableComponent() {
        List<Component> components = mEntity.getComponents();
        for (Component c : components) {
            if (c instanceof MovableComponent) {
                return (MovableComponent) c;
            }
        }
        return null;
    }

    @Test
    public void testCreate() {
        assertNotNull(mMovableComponent);
    }

    @Test
    public void testDispose() {
        mMovableComponent.setMovable(true, false);
        mMovableComponent.dispose();

        assertNull(getMovableComponent());
    }

    @Test
    public void testSetMovable_True() {
        mMovableComponent.setMovable(true, false);

        assertNotNull(getMovableComponent());
        assertEquals(1, mEntity.getComponents().size());
    }

    @Test
    public void testSetMovable_False() {
        mMovableComponent.setMovable(true, false);
        assertEquals(1, mEntity.getComponents().size());

        mMovableComponent.setMovable(false, false);
        assertEquals(0, mEntity.getComponents().size());
    }

    @Test
    public void testSetMovable_Twice() {
        mMovableComponent.setMovable(true, false);
        MovableComponent firstComponent = getMovableComponent();
        assertNotNull(firstComponent);

        mMovableComponent.setMovable(true, true);
        MovableComponent secondComponent = getMovableComponent();
        assertNotNull(secondComponent);

        assertFalse(firstComponent == secondComponent);
        assertEquals(1, mEntity.getComponents().size());
    }

    @Test
    public void testAddMoveListener() {
        mMovableComponent.setMovable(true, false);
        mMovableComponent.addMoveListener(mListener);

        assertNotNull(getMovableComponent());
    }

    @Test
    public void testAddMoveListener_Twice() {
        mMovableComponent.setMovable(true, false);
        mMovableComponent.addMoveListener(mListener);
        mMovableComponent.addMoveListener(mListener);

        assertTrue(mMovableComponent.hasMoveListenerForTesting(mListener));

        mMovableComponent.removeMoveListener(mListener);
        assertFalse(mMovableComponent.hasMoveListenerForTesting(mListener));
    }

    @Test
    public void testRemoveMoveListener() {
        mMovableComponent.setMovable(true, false);
        mMovableComponent.addMoveListener(mListener);

        assertNotNull(getMovableComponent());
        assertTrue(mMovableComponent.hasMoveListenerForTesting(mListener));

        mMovableComponent.removeMoveListener(mListener);
        assertFalse(mMovableComponent.hasMoveListenerForTesting(mListener));
    }
}
