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
import org.chromium.ui.xr.scenecore.XrInteractableComponent.OnClickListener;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link XrInteractableComponentImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrInteractableComponentImplTest {

    @Mock private OnClickListener mListener1;
    @Mock private OnClickListener mListener2;
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

    private InputEvent createInputEvent(InputEvent.Action action) {
        return new InputEvent(
                InputEvent.Source.HANDS,
                InputEvent.Pointer.RIGHT,
                /* timestamp= */ 0L,
                /* origin= */ new Vector3(0f, 0f, 0f),
                /* direction= */ new Vector3(0f, 0f, -1f),
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
}
