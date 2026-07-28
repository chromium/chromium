// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.activity.ComponentActivity;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.runtime.math.Vector3;
import androidx.xr.scenecore.Component;
import androidx.xr.scenecore.InputEvent;
import androidx.xr.scenecore.InteractableComponent;
import androidx.xr.scenecore.PanelEntity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;
import org.chromium.ui.xr.scenecore.XrInteractableComponent.OnClickListener;
import org.chromium.ui.xr.scenecore.XrVector3;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link XrInteractableComponentImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrInteractableComponentImplTest {
    static {
        XrModuleProviderImpl.initialize();
    }

    @Mock private OnClickListener mListener1;
    @Mock private OnClickListener mListener2;
    @Mock private XrInteractableComponent.OnDragListener mDragListener;
    @Mock private View mView;

    private Session mSession;
    private XrInteractableComponentImpl<PanelEntity> mInteractableComponent;
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
        mInteractableComponent = new XrInteractableComponentImpl<>(mSession, mEntity);
    }

    private InteractableComponent getInteractableComponent() {
        List<Component> components = mEntity.getComponents();
        for (Component c : components) {
            if (c instanceof InteractableComponent) {
                return (InteractableComponent) c;
            }
        }
        return null;
    }

    @Test
    public void testCreate() {
        assertNotNull(mInteractableComponent);
    }

    @Test
    public void testDispose() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.dispose();

        assertNull(getInteractableComponent());
    }

    @Test
    public void testSetInteractable_True() {
        mInteractableComponent.setInteractable(true);

        assertNotNull(getInteractableComponent());
        assertEquals(1, mEntity.getComponents().size());
    }

    @Test
    public void testSetInteractable_False() {
        mInteractableComponent.setInteractable(true);
        assertEquals(1, mEntity.getComponents().size());

        mInteractableComponent.setInteractable(false);
        assertEquals(0, mEntity.getComponents().size());
    }

    @Test
    public void testAddClickListener() {
        mInteractableComponent.addOnClickListener(mListener1);
        assertTrue(mInteractableComponent.hasOnClickListenerForTesting(mListener1));
    }

    @Test
    public void testRemoveClickListener() {
        mInteractableComponent.addOnClickListener(mListener1);
        assertTrue(mInteractableComponent.hasOnClickListenerForTesting(mListener1));

        mInteractableComponent.removeOnClickListener(mListener1);
        assertFalse(mInteractableComponent.hasOnClickListenerForTesting(mListener1));
    }

    @Test
    public void testAddDragListener() {
        mInteractableComponent.addOnDragListener(mDragListener);
        assertTrue(mInteractableComponent.hasOnDragListenerForTesting(mDragListener));
    }

    @Test
    public void testRemoveDragListener() {
        mInteractableComponent.addOnDragListener(mDragListener);
        assertTrue(mInteractableComponent.hasOnDragListenerForTesting(mDragListener));

        mInteractableComponent.removeOnDragListener(mDragListener);
        assertFalse(mInteractableComponent.hasOnDragListenerForTesting(mDragListener));
    }

    private InputEvent createInputEvent(InputEvent.Action action) {
        return createInputEvent(action, new Vector3(0f, 0f, 0f), new Vector3(0f, 0f, -1f));
    }

    private InputEvent createInputEvent(
            InputEvent.Action action, Vector3 origin, Vector3 direction) {
        return new InputEvent(
                InputEvent.Source.HANDS,
                InputEvent.Pointer.RIGHT,
                /* timestamp= */ 0L,
                origin,
                direction,
                action,
                /* hitInfoList= */ new ArrayList<>());
    }

    @Test
    public void testClickPropagation() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);
        mInteractableComponent.addOnClickListener(mListener2);

        // Simulate DOWN event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.DOWN));

        // Simulate UP event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.UP));

        verify(mListener1, times(1)).onClick();
        verify(mListener2, times(1)).onClick();
    }

    @Test
    public void testClickTimeout() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);

        // Simulate DOWN event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.DOWN));

        // Fast forward past click timeout (500ms)
        ShadowLooper.idleMainLooper(600, TimeUnit.MILLISECONDS);

        // Simulate UP event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.UP));

        // Click should have timed out, listener not called
        verify(mListener1, never()).onClick();
    }

    @Test
    public void testClickCancelled() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);

        // Simulate DOWN event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.DOWN));

        // Simulate CANCEL event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.CANCEL));

        // Simulate UP event
        mInteractableComponent.onInputEvent(createInputEvent(InputEvent.Action.UP));

        // Click was cancelled, listener not called
        verify(mListener1, never()).onClick();
    }

    @Test
    public void testClickNotSuppressedOnSmallMove() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.DOWN, new Vector3(0f, 0f, 0f), new Vector3(0f, 0f, -1f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.MOVE,
                        new Vector3(0.01f, 0f, 0f),
                        new Vector3(0.01f, 0f, -0.9999f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.UP,
                        new Vector3(0.01f, 0f, 0f),
                        new Vector3(0.01f, 0f, -0.9999f)));

        verify(mListener1, times(1)).onClick();
    }

    @Test
    public void testClickSuppressedOnLargeMove() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.DOWN, new Vector3(0f, 0f, 0f), new Vector3(0f, 0f, -1f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.MOVE,
                        new Vector3(0.12f, 0f, 0f),
                        new Vector3(0f, 0f, -1f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.UP,
                        new Vector3(0.12f, 0f, 0f),
                        new Vector3(0f, 0f, -1f)));

        verify(mListener1, never()).onClick();
    }

    @Test
    public void testClickSuppressedOnLargeDirectionChange() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.DOWN, new Vector3(0f, 0f, 0f), new Vector3(0f, 0f, -1f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.MOVE,
                        new Vector3(0f, 0f, 0f),
                        new Vector3(0.08f, 0f, -0.9968f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.UP,
                        new Vector3(0f, 0f, 0f),
                        new Vector3(0.08f, 0f, -0.9968f)));

        verify(mListener1, never()).onClick();
    }

    @Test
    public void testDragNotStartedOnSmallMove() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);
        mInteractableComponent.addOnDragListener(mDragListener);

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.DOWN, new Vector3(0f, 0f, 0f), new Vector3(0f, 0f, -1f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.MOVE,
                        new Vector3(0.01f, 0f, 0f),
                        new Vector3(0.01f, 0f, -0.9999f)));

        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.UP,
                        new Vector3(0.01f, 0f, 0f),
                        new Vector3(0.01f, 0f, -0.9999f)));

        verify(mListener1, times(1)).onClick();

        verify(mDragListener, never())
                .onDragStart(
                        org.mockito.ArgumentMatchers.any(XrVector3.class),
                        org.mockito.ArgumentMatchers.any(XrVector3.class));
        verify(mDragListener, never())
                .onDragUpdate(
                        org.mockito.ArgumentMatchers.any(XrVector3.class),
                        org.mockito.ArgumentMatchers.any(XrVector3.class));
        verify(mDragListener, never())
                .onDragEnd(
                        org.mockito.ArgumentMatchers.any(XrVector3.class),
                        org.mockito.ArgumentMatchers.any(XrVector3.class));
    }

    @Test
    public void testDragStartedOnLargeMove() {
        mInteractableComponent.setInteractable(true);
        mInteractableComponent.addOnClickListener(mListener1);
        mInteractableComponent.addOnDragListener(mDragListener);

        float[] downOrigin = new float[] {0f, 0f, 0f};
        float[] downDir = new float[] {0f, 0f, -1f};
        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.DOWN,
                        new Vector3(downOrigin[0], downOrigin[1], downOrigin[2]),
                        new Vector3(downDir[0], downDir[1], downDir[2])));

        float[] moveOrigin = new float[] {0.12f, 0f, 0f};
        float[] moveDir = new float[] {0f, 0f, -1f};
        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.MOVE,
                        new Vector3(moveOrigin[0], moveOrigin[1], moveOrigin[2]),
                        new Vector3(moveDir[0], moveDir[1], moveDir[2])));

        float[] upOrigin = new float[] {0.13f, 0f, 0f};
        float[] upDir = new float[] {0f, 0f, -1f};
        mInteractableComponent.onInputEvent(
                createInputEvent(
                        InputEvent.Action.UP,
                        new Vector3(upOrigin[0], upOrigin[1], upOrigin[2]),
                        new Vector3(upDir[0], upDir[1], upDir[2])));

        verify(mListener1, never()).onClick();

        verify(mDragListener, times(1))
                .onDragStart(
                        org.mockito.ArgumentMatchers.eq(XrVector3.create(0.12f, 0f, 0f)),
                        org.mockito.ArgumentMatchers.eq(XrVector3.create(0f, 0f, -1f)));
        verify(mDragListener, times(1))
                .onDragUpdate(
                        org.mockito.ArgumentMatchers.eq(XrVector3.create(0.12f, 0f, 0f)),
                        org.mockito.ArgumentMatchers.eq(XrVector3.create(0f, 0f, -1f)));
        verify(mDragListener, times(1))
                .onDragEnd(
                        org.mockito.ArgumentMatchers.eq(XrVector3.create(0.13f, 0f, 0f)),
                        org.mockito.ArgumentMatchers.eq(XrVector3.create(0f, 0f, -1f)));
    }
}
