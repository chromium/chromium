// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.PointF;
import android.view.DragEvent;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler.AnimatedDragShadowBuilder;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler.DragHandlerDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** Unit tests for {@link TabSwitcherDragHandler} and {@link AnimatedDragShadowBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSwitcherDragHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private DragAndDropDelegate mDragAndDropDelegate;
    @Mock private TabSwitcherBackPressHandlerManager mDragHandlerManager;
    @Mock private DragHandlerDelegate mDragHandlerDelegate;
    @Mock private Canvas mCanvas;

    private TabSwitcherDragHandler mDragHandler;

    @Before
    public void setUp() {
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        mDragHandler =
                new TabSwitcherDragHandler(
                        () -> mActivity,
                        mMultiInstanceManager,
                        mDragAndDropDelegate,
                        mDragHandlerManager);
        mDragHandler.setDragHandlerDelegate(mDragHandlerDelegate);
    }

    @Test
    public void testDragHandlerDelegate_DefaultMethodForwarding() {
        class TestDelegate implements DragHandlerDelegate {
            boolean mDragStartCalled;
            boolean mDragEndCalled;
            boolean mDragLocationCalled;
            boolean mDropCalled;

            @Override
            public boolean handleDragStart(float xPx, float yPx) {
                mDragStartCalled = true;
                return true;
            }

            @Override
            public boolean handleExternalDragEnd(float xPx, float yPx, boolean isOSNewWindowDrop) {
                mDragEndCalled = true;
                return true;
            }

            @Override
            public boolean handleDragLocation(float xPx, float yPx) {
                mDragLocationCalled = true;
                return true;
            }

            @Override
            public boolean handleDrop(float xPx, float yPx) {
                mDropCalled = true;
                return true;
            }
        }

        TestDelegate delegate = new TestDelegate();
        View view = new View(ContextUtils.getApplicationContext());

        assertTrue(delegate.handleDragStart(view, 10f, 20f));
        assertTrue(delegate.mDragStartCalled);

        assertTrue(delegate.handleExternalDragEnd(view, 10f, 20f, false));
        assertTrue(delegate.mDragEndCalled);

        assertTrue(delegate.handleDragLocation(view, 10f, 20f));
        assertTrue(delegate.mDragLocationCalled);

        assertTrue(delegate.handleDrop(view, 10f, 20f));
        assertTrue(delegate.mDropCalled);
    }

    @Test
    public void testAnimatedDragShadowBuilder_ShowAndHideShadow() {
        View originalView = spy(new View(ContextUtils.getApplicationContext()));
        View shadowView = spy(new View(ContextUtils.getApplicationContext()));
        shadowView.layout(0, 0, 100, 200);

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(originalView, shadowView, new PointF(10f, 20f), 0L);

        // Visible by default
        Point shadowSize = new Point();
        Point shadowTouchPoint = new Point();
        builder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        assertEquals(100, shadowSize.x);
        assertEquals(200, shadowSize.y);

        builder.onDrawShadow(mCanvas);
        verify(shadowView).draw(mCanvas);

        // Hide shadow
        builder.update(null, /* show= */ false);
        builder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        assertEquals(1, shadowSize.x);
        assertEquals(1, shadowSize.y);
        assertEquals(0, shadowTouchPoint.x);
        assertEquals(0, shadowTouchPoint.y);

        clearInvocations(shadowView);
        builder.onDrawShadow(mCanvas);
        verify(shadowView, never()).draw(any());

        // Restore shadow
        builder.update(null, /* show= */ true);
        builder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        assertEquals(100, shadowSize.x);
        assertEquals(200, shadowSize.y);
    }

    @Test
    public void testAnimatedDragShadowBuilder_ViewResolutionChain() {
        View attachedView = spy(new View(ContextUtils.getApplicationContext()));
        doReturn(true).when(attachedView).isAttachedToWindow();

        View originalView = spy(new View(ContextUtils.getApplicationContext()));
        doReturn(false).when(originalView).isAttachedToWindow();

        View shadowView = spy(new View(ContextUtils.getApplicationContext()));
        doReturn(false).when(shadowView).isAttachedToWindow();

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(originalView, shadowView, new PointF(0f, 0f), 0L);

        // 1. Attached view provided explicitly
        builder.update(attachedView, /* show= */ false);
        verify(attachedView).updateDragShadow(builder);

        // 2. Fallback to attached original view
        clearInvocations(attachedView);
        doReturn(true).when(originalView).isAttachedToWindow();
        builder.update(null, /* show= */ true);
        verify(originalView).updateDragShadow(builder);

        // 3. Fallback to original view's root view
        doReturn(false).when(originalView).isAttachedToWindow();
        View originalRootView = spy(new View(ContextUtils.getApplicationContext()));
        doReturn(true).when(originalRootView).isAttachedToWindow();
        doReturn(originalRootView).when(originalView).getRootView();
        builder.update(null, /* show= */ false);
        verify(originalRootView).updateDragShadow(builder);

        // 4. Fallback to attached shadow view
        doReturn(null).when(originalView).getRootView();
        doReturn(true).when(shadowView).isAttachedToWindow();
        builder.update(null, /* show= */ true);
        verify(shadowView).updateDragShadow(builder);

        // 5. Fallback to shadow view's root view
        doReturn(false).when(shadowView).isAttachedToWindow();
        View shadowRootView = spy(new View(ContextUtils.getApplicationContext()));
        doReturn(true).when(shadowRootView).isAttachedToWindow();
        doReturn(shadowRootView).when(shadowView).getRootView();
        builder.update(null, /* show= */ false);
        verify(shadowRootView).updateDragShadow(builder);
    }

    @Test
    public void testShowDragShadow_ViaGlobalState() {
        View originalView = spy(new View(ContextUtils.getApplicationContext()));
        doReturn(true).when(originalView).isAttachedToWindow();
        View shadowView = spy(new View(ContextUtils.getApplicationContext()));

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(originalView, shadowView, new PointF(0f, 0f), 0L);
        DropDataAndroid dropData = mock(DropDataAndroid.class);
        Token token = DragDropGlobalState.store(1, dropData, builder);

        mDragHandler.showDragShadow(false);
        verify(originalView).updateDragShadow(builder);

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DispatchesViewToDelegate() {
        View targetView = new View(ContextUtils.getApplicationContext());

        // ACTION_DRAG_LOCATION
        DragEvent dragLocationEvent = mock(DragEvent.class);
        when(dragLocationEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_LOCATION);
        when(dragLocationEvent.getX()).thenReturn(50f);
        when(dragLocationEvent.getY()).thenReturn(60f);

        mDragHandler.onDrag(targetView, dragLocationEvent);
        verify(mDragHandlerDelegate).handleDragLocation(targetView, 50f, 60f);

        // ACTION_DRAG_ENTERED
        DragEvent dragEnterEvent = mock(DragEvent.class);
        when(dragEnterEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENTERED);
        mDragHandler.onDrag(targetView, dragEnterEvent);
        verify(mDragHandlerDelegate).handleDragEnter();

        // ACTION_DRAG_EXITED
        DragEvent dragExitEvent = mock(DragEvent.class);
        when(dragExitEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_EXITED);
        mDragHandler.onDrag(targetView, dragExitEvent);
        verify(mDragHandlerDelegate).handleDragExit();

        // ACTION_DROP
        DropDataAndroid dropData = mock(DropDataAndroid.class);
        Token token = DragDropGlobalState.store(1, dropData, null);
        DragEvent dropEvent = mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(dropEvent.getX()).thenReturn(70f);
        when(dropEvent.getY()).thenReturn(80f);
        when(mDragHandlerDelegate.handleDrop(targetView, 70f, 80f)).thenReturn(true);
        mDragHandler.onDrag(targetView, dropEvent);
        verify(mDragHandlerDelegate).handleDrop(targetView, 70f, 80f);
        DragDropGlobalState.clear(token);

        // ACTION_DRAG_ENDED
        DragEvent dragEndEvent = mock(DragEvent.class);
        when(dragEndEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        when(dragEndEvent.getX()).thenReturn(90f);
        when(dragEndEvent.getY()).thenReturn(100f);
        mDragHandler.onDrag(targetView, dragEndEvent);
        verify(mDragHandlerDelegate).handleExternalDragEnd(targetView, 90f, 100f, false);
    }
}
