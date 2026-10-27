// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipDescription;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.drawable.Drawable;
import android.view.DragEvent;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler.AnimatedDragShadowBuilder;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler.DragHandlerDelegate;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DropDataAndroid;

import java.util.Collections;

/** Unit tests for {@link TabSwitcherDragHandler} and {@link AnimatedDragShadowBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherDragHandlerUnitTest {
    @Implements(View.class)
    public static class ShadowDragView extends ShadowView {
        private static @Nullable View sLastCancelledDragView;
        private static @Nullable View sLastUpdatedDragShadowView;

        @Resetter
        public static void reset() {
            sLastCancelledDragView = null;
            sLastUpdatedDragShadowView = null;
        }

        @Implementation
        protected void cancelDragAndDrop() {
            sLastCancelledDragView = realView;
        }

        @Implementation
        protected void updateDragShadow(View.DragShadowBuilder shadowBuilder) {
            sLastUpdatedDragShadowView = realView;
        }
    }

    private static class TestDelegate implements DragHandlerDelegate {
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private DragAndDropDelegate mDragAndDropDelegate;
    @Mock private TabSwitcherBackPressHandlerManager mDragHandlerManager;
    @Mock private DragHandlerDelegate mDragHandlerDelegate;
    @Mock private Canvas mCanvas;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;

    private Activity mActivity;
    private FrameLayout mRootView;
    private TabSwitcherDragHandler mDragHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(mRootView);

        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        SettableMonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);
        when(mTabModelSelector.getModels()).thenReturn(Collections.singletonList(mTabModel));
        currentTabModelSupplier.set(mTabModel);

        mDragHandler =
                new TabSwitcherDragHandler(
                        () -> mActivity,
                        mMultiInstanceManager,
                        mDragAndDropDelegate,
                        mDragHandlerManager,
                        /* fadeDragShadow= */ true);
        mDragHandler.setDragHandlerDelegate(mDragHandlerDelegate);
    }

    @After
    public void tearDown() {
        DragDropGlobalState.clearForTesting();
    }

    @Test
    public void testDragHandlerDelegate_DefaultMethodForwarding() {
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
        View originalView = new View(mActivity);
        View shadowView = new View(mActivity);
        Drawable shadowBackground = mock(Drawable.class);
        shadowView.setBackground(shadowBackground);
        shadowView.layout(/* left= */ 0, /* top= */ 0, /* right= */ 100, /* bottom= */ 200);

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(
                        originalView,
                        shadowView,
                        new PointF(10f, 20f),
                        /* animationDuration= */ 0L,
                        /* fadeDragShadow= */ true);

        // Visible by default
        Point shadowSize = new Point();
        Point shadowTouchPoint = new Point();
        builder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        assertEquals(100, shadowSize.x);
        assertEquals(200, shadowSize.y);

        builder.onDrawShadow(mCanvas);
        verify(shadowBackground).draw(mCanvas);

        // Hide shadow
        builder.update(/* attachedView= */ null, /* show= */ false);
        builder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        assertEquals(1, shadowSize.x);
        assertEquals(1, shadowSize.y);
        assertEquals(0, shadowTouchPoint.x);
        assertEquals(0, shadowTouchPoint.y);

        clearInvocations(shadowBackground);
        builder.onDrawShadow(mCanvas);
        verify(shadowBackground, never()).draw(any());

        // Restore shadow
        builder.update(/* attachedView= */ null, /* show= */ true);
        builder.onProvideShadowMetrics(shadowSize, shadowTouchPoint);
        assertEquals(100, shadowSize.x);
        assertEquals(200, shadowSize.y);
    }

    @Test
    public void testAnimatedDragShadowBuilder_FadeDragShadowDisabled() {
        View originalView = new View(mActivity);
        View shadowView = new View(mActivity);
        Drawable shadowBackground = mock(Drawable.class);
        shadowView.setBackground(shadowBackground);
        shadowView.setLayoutParams(
                new FrameLayout.LayoutParams(/* width= */ 100, /* height= */ 200));
        mRootView.addView(shadowView);
        shadowView.layout(/* left= */ 0, /* top= */ 0, /* right= */ 100, /* bottom= */ 200);

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(
                        originalView,
                        shadowView,
                        new PointF(10f, 20f),
                        /* animationDuration= */ 0L,
                        /* fadeDragShadow= */ false);

        // When fading is disabled, animate() is not posted and the view is drawn directly.
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(100, shadowView.getLayoutParams().width);
        assertEquals(200, shadowView.getLayoutParams().height);
        builder.onDrawShadow(mCanvas);
        verify(mCanvas, never()).saveLayer(anyFloat(), anyFloat(), anyFloat(), anyFloat(), any());
        verify(shadowBackground).draw(mCanvas);
    }

    @Test
    @Config(shadows = TabSwitcherDragHandlerUnitTest.ShadowDragView.class)
    public void testAnimatedDragShadowBuilder_ViewResolutionChain() {
        Activity otherActivity = Robolectric.buildActivity(Activity.class).setup().get();
        View otherWindowView = new View(otherActivity);
        otherActivity.setContentView(otherWindowView);

        View attachedView = new View(mActivity);
        View originalView = new View(mActivity);
        View shadowView = new View(mActivity);

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(
                        originalView,
                        shadowView,
                        new PointF(0f, 0f),
                        /* animationDuration= */ 0L,
                        /* fadeDragShadow= */ true);

        // 1. Original view is preferred over an attached view, which may be in another window.
        mRootView.addView(originalView);
        builder.update(otherWindowView, /* show= */ false);
        assertSame(originalView, ShadowDragView.sLastUpdatedDragShadowView);

        // 2. Fallback to attached shadow view
        ShadowDragView.reset();
        mRootView.removeView(originalView);
        mRootView.addView(shadowView);
        builder.update(otherWindowView, /* show= */ true);
        assertSame(shadowView, ShadowDragView.sLastUpdatedDragShadowView);

        // 3. Last resort: the explicitly provided attached view
        ShadowDragView.reset();
        mRootView.removeView(shadowView);
        mRootView.addView(attachedView);
        builder.update(attachedView, /* show= */ false);
        assertSame(attachedView, ShadowDragView.sLastUpdatedDragShadowView);

        // 4. No-op when no view is attached
        ShadowDragView.reset();
        mRootView.removeView(attachedView);
        builder.update(attachedView, /* show= */ true);
        assertNull(ShadowDragView.sLastUpdatedDragShadowView);
    }

    @Test
    @Config(shadows = TabSwitcherDragHandlerUnitTest.ShadowDragView.class)
    public void testShowDragShadow_ViaGlobalState() {
        View originalView = new View(mActivity);
        mRootView.addView(originalView);
        View shadowView = new View(mActivity);

        AnimatedDragShadowBuilder builder =
                new AnimatedDragShadowBuilder(
                        originalView,
                        shadowView,
                        new PointF(0f, 0f),
                        /* animationDuration= */ 0L,
                        /* fadeDragShadow= */ true);
        DropDataAndroid dropData = mock(DropDataAndroid.class);
        Token token = DragDropGlobalState.store(1, dropData, builder);

        mDragHandler.showDragShadow(/* show= */ false);
        assertSame(originalView, ShadowDragView.sLastUpdatedDragShadowView);

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DispatchesViewToDelegate() {
        mDragHandler.setTabModelSelector(mTabModelSelector);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

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
        verify(mDragHandlerDelegate).handleDragEnter(targetView);

        // ACTION_DRAG_EXITED
        DragEvent dragExitEvent = mock(DragEvent.class);
        when(dragExitEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_EXITED);
        mDragHandler.onDrag(targetView, dragExitEvent);
        verify(mDragHandlerDelegate).handleDragExit(targetView);

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

    @Test
    public void testOnDrag_IncognitoMismatch_Rejected() {
        // Destination window is regular (non-incognito).
        mDragHandler.setTabModelSelector(mTabModelSelector);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        // Dragged item is incognito.
        Tab incognitoTab = mock(Tab.class);
        when(incognitoTab.isIncognitoBranded()).thenReturn(true);
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(incognitoTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View targetView = new View(ContextUtils.getApplicationContext());
        ClipDescription clipDescription =
                new ClipDescription("tab", new String[] {MimeTypeUtils.CHROME_MIMETYPE_TAB});

        // ACTION_DRAG_STARTED
        DragEvent dragStartEvent = mock(DragEvent.class);
        when(dragStartEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(dragStartEvent.getClipDescription()).thenReturn(clipDescription);
        assertFalse(
                "ACTION_DRAG_STARTED must return false on incognito mismatch.",
                mDragHandler.onDrag(targetView, dragStartEvent));
        verify(mDragHandlerDelegate, never())
                .handleDragStart(any(View.class), anyFloat(), anyFloat());

        // ACTION_DRAG_ENTERED
        DragEvent dragEnterEvent = mock(DragEvent.class);
        when(dragEnterEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENTERED);
        assertFalse(
                "ACTION_DRAG_ENTERED must return false on incognito mismatch.",
                mDragHandler.onDrag(targetView, dragEnterEvent));
        verify(mDragHandlerDelegate, never()).handleDragEnter(any(View.class));

        // ACTION_DRAG_LOCATION
        DragEvent dragLocationEvent = mock(DragEvent.class);
        when(dragLocationEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_LOCATION);
        assertFalse(
                "ACTION_DRAG_LOCATION must return false on incognito mismatch.",
                mDragHandler.onDrag(targetView, dragLocationEvent));
        verify(mDragHandlerDelegate, never())
                .handleDragLocation(any(View.class), anyFloat(), anyFloat());

        // ACTION_DROP
        DragEvent dropEvent = mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        assertFalse(
                "ACTION_DROP must return false on incognito mismatch.",
                mDragHandler.onDrag(targetView, dropEvent));
        verify(mDragHandlerDelegate, never()).handleDrop(any(View.class), anyFloat(), anyFloat());
        assertFalse(DragDropGlobalState.didChromeHandleDrop());

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_IncognitoMatch_Accepted() {
        // Destination window is incognito.
        mDragHandler.setTabModelSelector(mTabModelSelector);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(true);

        // Dragged item is incognito.
        Tab incognitoTab = mock(Tab.class);
        when(incognitoTab.isIncognitoBranded()).thenReturn(true);
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(incognitoTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View targetView = new View(ContextUtils.getApplicationContext());
        ClipDescription clipDescription =
                new ClipDescription("tab", new String[] {MimeTypeUtils.CHROME_MIMETYPE_TAB});

        // ACTION_DRAG_STARTED
        DragEvent dragStartEvent = mock(DragEvent.class);
        when(dragStartEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(dragStartEvent.getClipDescription()).thenReturn(clipDescription);
        when(dragStartEvent.getX()).thenReturn(10f);
        when(dragStartEvent.getY()).thenReturn(20f);
        when(mDragHandlerDelegate.handleDragStart(targetView, 10f, 20f)).thenReturn(true);
        assertTrue(
                "ACTION_DRAG_STARTED must return true when incognito matches.",
                mDragHandler.onDrag(targetView, dragStartEvent));
        verify(mDragHandlerDelegate).handleDragStart(targetView, 10f, 20f);

        // ACTION_DRAG_ENTERED
        DragEvent dragEnterEvent = mock(DragEvent.class);
        when(dragEnterEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENTERED);
        when(mDragHandlerDelegate.handleDragEnter(targetView)).thenReturn(true);
        assertTrue(
                "ACTION_DRAG_ENTERED must return true when incognito matches.",
                mDragHandler.onDrag(targetView, dragEnterEvent));
        verify(mDragHandlerDelegate).handleDragEnter(targetView);

        // ACTION_DRAG_LOCATION
        DragEvent dragLocationEvent = mock(DragEvent.class);
        when(dragLocationEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_LOCATION);
        when(dragLocationEvent.getX()).thenReturn(30f);
        when(dragLocationEvent.getY()).thenReturn(40f);
        when(mDragHandlerDelegate.handleDragLocation(targetView, 30f, 40f)).thenReturn(true);
        assertTrue(
                "ACTION_DRAG_LOCATION must return true when incognito matches.",
                mDragHandler.onDrag(targetView, dragLocationEvent));
        verify(mDragHandlerDelegate).handleDragLocation(targetView, 30f, 40f);

        // ACTION_DROP
        DragEvent dropEvent = mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(dropEvent.getX()).thenReturn(50f);
        when(dropEvent.getY()).thenReturn(60f);
        when(mDragHandlerDelegate.handleDrop(targetView, 50f, 60f)).thenReturn(true);
        assertTrue(
                "ACTION_DROP must return true when incognito matches.",
                mDragHandler.onDrag(targetView, dropEvent));
        verify(mDragHandlerDelegate).handleDrop(targetView, 50f, 60f);
        assertTrue(DragDropGlobalState.didChromeHandleDrop());

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testDoesBelongToCurrentModel_NullSelectorOrModel_FailsClosed() {
        // TabModelSelector not set
        assertFalse(mDragHandler.doesBelongToCurrentModel(false));
        assertFalse(mDragHandler.doesBelongToCurrentModel(true));

        // TabModelSelector set, but getCurrentModel() is null
        mDragHandler.setTabModelSelector(mTabModelSelector);
        when(mTabModelSelector.getCurrentModel()).thenReturn(null);
        assertFalse(mDragHandler.doesBelongToCurrentModel(false));
        assertFalse(mDragHandler.doesBelongToCurrentModel(true));
    }

    @Test
    public void testIsDragSourceInstance() {
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(1);

        // No active drag -> false
        assertFalse(mDragHandler.isDragSourceInstance());

        DropDataAndroid dropData = mock(DropDataAndroid.class);
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        // Active drag originated from instance 1 -> true
        assertTrue(mDragHandler.isDragSourceInstance());

        // Different instance
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);
        assertFalse(mDragHandler.isDragSourceInstance());

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DragEnded_OSNewWindowDrop_DoesNotRestoreSourceAlpha() {
        View dragSourceView = new View(ContextUtils.getApplicationContext());
        dragSourceView.setAlpha(0f);
        mDragHandler.mDragSourceView = dragSourceView;

        Tab tab = mock(Tab.class);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View targetView = new View(ContextUtils.getApplicationContext());
        DragEvent dragEndEvent = mock(DragEvent.class);
        when(dragEndEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        when(dragEndEvent.getResult()).thenReturn(true);
        when(dragEndEvent.getX()).thenReturn(10f);
        when(dragEndEvent.getY()).thenReturn(20f);

        mDragHandler.onDrag(targetView, dragEndEvent);

        // Alpha should NOT be restored to 1.0 on OS new window drops to prevent ghost tabs.
        assertEquals(0f, dragSourceView.getAlpha(), 0.0f);
        verify(mDragHandlerDelegate).handleExternalDragEnd(targetView, 10f, 20f, true);

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DragEnded_CrossWindowDrop_DoesNotRestoreSourceAlpha() {
        View dragSourceView = new View(ContextUtils.getApplicationContext());
        dragSourceView.setAlpha(0f);
        mDragHandler.mDragSourceView = dragSourceView;

        Tab tab = mock(Tab.class);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        // Simulate drop handled by another window
        DragEvent dropEvent = mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        DragDropGlobalState.notifyChromeHandledDrop(dropEvent);

        View targetView = new View(ContextUtils.getApplicationContext());
        DragEvent dragEndEvent = mock(DragEvent.class);
        when(dragEndEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        when(dragEndEvent.getResult()).thenReturn(true);
        when(dragEndEvent.getX()).thenReturn(10f);
        when(dragEndEvent.getY()).thenReturn(20f);

        mDragHandler.onDrag(targetView, dragEndEvent);

        // Alpha should NOT be restored to 1.0 on cross-window drops to prevent ghost tabs.
        assertEquals(0f, dragSourceView.getAlpha(), 0.0f);
        verify(mDragHandlerDelegate).handleExternalDragEnd(targetView, 10f, 20f, true);

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DragEnded_NonOSNewWindowDrop_RestoresSourceAlpha() {
        View dragSourceView = new View(ContextUtils.getApplicationContext());
        dragSourceView.setAlpha(0f);
        mDragHandler.mDragSourceView = dragSourceView;

        Tab tab = mock(Tab.class);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View targetView = new View(ContextUtils.getApplicationContext());
        DragEvent dragEndEvent = mock(DragEvent.class);
        when(dragEndEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        when(dragEndEvent.getResult()).thenReturn(false);
        when(dragEndEvent.getX()).thenReturn(10f);
        when(dragEndEvent.getY()).thenReturn(20f);

        mDragHandler.onDrag(targetView, dragEndEvent);

        // Alpha should be restored to 1.0 immediately for non-new-window drops.
        assertEquals(1f, dragSourceView.getAlpha(), 0.0f);
        verify(mDragHandlerDelegate).handleExternalDragEnd(targetView, 10f, 20f, false);

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DragEnded_SameWindowDrop_RestoresSourceAlpha() {
        mDragHandler.setTabModelSelector(mTabModelSelector);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        View dragSourceView = new View(ContextUtils.getApplicationContext());
        dragSourceView.setAlpha(0f);
        mDragHandler.mDragSourceView = dragSourceView;

        Tab tab = mock(Tab.class);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View targetView = new View(ContextUtils.getApplicationContext());
        when(mDragHandlerDelegate.handleDrop(targetView, 10f, 20f)).thenReturn(true);

        // ACTION_DROP handled by this handler
        DragEvent dropEvent = mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(dropEvent.getX()).thenReturn(10f);
        when(dropEvent.getY()).thenReturn(20f);
        mDragHandler.onDrag(targetView, dropEvent);

        // ACTION_DRAG_ENDED
        DragEvent dragEndEvent = mock(DragEvent.class);
        when(dragEndEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        when(dragEndEvent.getResult()).thenReturn(true);
        when(dragEndEvent.getX()).thenReturn(10f);
        when(dragEndEvent.getY()).thenReturn(20f);

        mDragHandler.onDrag(targetView, dragEndEvent);

        // Alpha should be restored to 1.0 because this handler handled the drop.
        assertEquals(1f, dragSourceView.getAlpha(), 0.0f);
        verify(mDragHandlerDelegate).handleExternalDragEnd(targetView, 10f, 20f, false);

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testOnDrag_DragEnded_WithNullGlobalState_DoesNotCrash() {
        View targetView = new View(ContextUtils.getApplicationContext());

        // Set mDragSourceView on the handler to simulate state before drag end, but with null
        // global state.
        mDragHandler.mDragSourceView = targetView;

        DragEvent dragEndEvent = mock(DragEvent.class);
        when(dragEndEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        when(dragEndEvent.getResult()).thenReturn(true);
        when(dragEndEvent.getX()).thenReturn(50f);
        when(dragEndEvent.getY()).thenReturn(50f);

        // onDrag must complete without throwing AssertionError.
        mDragHandler.onDrag(targetView, dragEndEvent);
        verify(mDragHandlerDelegate).handleExternalDragEnd(targetView, 50f, 50f, false);
    }

    @Test
    public void testHasActiveDragShadow() {
        assertFalse(mDragHandler.hasActiveDragShadow());
    }

    @Test
    @Config(shadows = TabSwitcherDragHandlerUnitTest.ShadowDragView.class)
    public void testHandleEscPress_ExternalDragInProgress_CancelsDragAndDrop() {
        Token token = new Token(1L, 2L);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View dragSourceView = new View(mActivity);
        mRootView.addView(dragSourceView);
        mDragHandler.mDragSourceView = dragSourceView;
        when(mDragHandlerDelegate.isDragInProcess()).thenReturn(true);

        assertTrue(mDragHandler.handleEscPress());
        assertSame(dragSourceView, ShadowDragView.sLastCancelledDragView);
        verify(mDragHandlerDelegate, never()).handleInternalDragEnd();
    }

    @Test
    public void testHandleEscPress_InternalDragInProgress_DelegatesToInternalDragEnd() {
        TabDragHandlerBase.setDragTokenForTesting(null);
        when(mDragHandlerDelegate.isDragInProcess()).thenReturn(true);
        when(mDragHandlerDelegate.handleInternalDragEnd())
                .thenReturn(BackPressHandler.BackPressResult.SUCCESS);

        assertTrue(mDragHandler.handleEscPress());
        verify(mDragHandlerDelegate).handleInternalDragEnd();
    }

    @Test
    public void testHandleEscPress_InternalDragInProgress_Failure() {
        TabDragHandlerBase.setDragTokenForTesting(null);
        when(mDragHandlerDelegate.isDragInProcess()).thenReturn(true);
        when(mDragHandlerDelegate.handleInternalDragEnd())
                .thenReturn(BackPressHandler.BackPressResult.FAILURE);

        assertFalse(mDragHandler.handleEscPress());
        verify(mDragHandlerDelegate).handleInternalDragEnd();
    }

    @Test
    public void testHandleEscPress_NoDragInProgress_FallsBackToSuper() {
        TabDragHandlerBase.setDragTokenForTesting(null);
        when(mDragHandlerDelegate.isDragInProcess()).thenReturn(false);
        mDragHandler.mDragSourceView = null;

        assertFalse(mDragHandler.handleEscPress());
        verify(mDragHandlerDelegate, never()).handleInternalDragEnd();
    }

    @Test
    public void testDestroy_RendersHandlerInert() {
        mDragHandler.destroy();

        DragEvent dragEvent = mock(DragEvent.class);
        when(dragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        assertFalse(mDragHandler.onDrag(new View(ContextUtils.getApplicationContext()), dragEvent));

        assertFalse(mDragHandler.handleEscPress());
        verify(mDragHandlerDelegate, never()).handleDragStart(any(), anyFloat(), anyFloat());
        verify(mDragHandlerDelegate, never()).isDragInProcess();
    }
}
