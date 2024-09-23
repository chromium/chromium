// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.ContentGestureListener.GestureState;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabHandleStrategy.DragEventCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content.browser.RenderCoordinatesImpl;

import java.util.function.BooleanSupplier;

/** Tests for {@link ContentGestureListener}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class ContentGestureListenerTest {
    private static final float DISTX = 0.f;

    @Mock private Tab mTab;
    @Mock private BooleanSupplier mIsFullyExpanded;
    @Mock private DragEventCallback mCallback;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private ContentView mTabContentView;
    @Mock private MotionEvent mEventSrc;

    private ContentGestureListener mListener;
    private MotionEvent mEventTo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mListener = new ContentGestureListener(() -> mTab, mCallback, mIsFullyExpanded);
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        when(mTab.getContentView()).thenReturn(mTabContentView);

        mListener.onDown(Mockito.mock(MotionEvent.class));
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        mEventTo = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 0, 0, 0);
    }

    @Test
    public void draggingTabSwitchesToScrollingContent() {
        // Verify dragging-up gesture expands tab.
        mListener.onScroll(mEventSrc, mEventTo, DISTX, 100.f);
        verify(mCallback).onDragStart(anyInt());
        assertEquals(GestureState.DRAG_TAB, mListener.getStateForTesting());

        // Make the tab fully expanded.
        when(mIsFullyExpanded.getAsBoolean()).thenReturn(true);
        when(mRenderCoordinates.getScrollYPixInt())
                .thenReturn(10); // Now not scrolled to the top any more.

        // Keep dragging up, and the mode switches to content scrolling.
        mListener.onScroll(mEventSrc, mEventTo, DISTX, 50.f);
        mListener.onScroll(mEventSrc, mEventTo, DISTX, 40.f);
        verify(mTabContentView).onTouchEvent(any(MotionEvent.class));
        assertEquals(GestureState.SCROLL_CONTENT, mListener.getStateForTesting());
    }

    @Test
    public void tabFling() {
        // Verify dragging-up gesture expands tab.
        mListener.onScroll(mEventSrc, mEventTo, DISTX, 100.f);
        verify(mCallback).onDragStart(anyInt());
        assertEquals(GestureState.DRAG_TAB, mListener.getStateForTesting());

        mListener.onFling(mEventSrc, mEventTo, DISTX, 100.f);
        verify(mCallback).onDragEnd(anyInt());
    }

    @Test
    public void dragDownFromExpandedState_movesTab() {
        when(mIsFullyExpanded.getAsBoolean()).thenReturn(true);

        // Verify dragging down gesture moves the tab down.
        mListener.onScroll(mEventSrc, mEventTo, DISTX, -50.f);
        verify(mCallback).onDragStart(anyInt());
        assertEquals(GestureState.DRAG_TAB, mListener.getStateForTesting());
    }

    @Test
    public void dragDownFromExpandedState_scrollsContents() {
        when(mIsFullyExpanded.getAsBoolean()).thenReturn(true);
        when(mRenderCoordinates.getScrollYPixInt())
                .thenReturn(10); // Not scrolled to the top any more.

        // Verify dragging down gesture moves the tab down.
        mListener.onScroll(mEventSrc, mEventTo, DISTX, -50.f);
        mListener.onScroll(mEventSrc, mEventTo, DISTX, -50.f);
        assertEquals(GestureState.SCROLL_CONTENT, mListener.getStateForTesting());
    }

    @Test
    public void dragUpFromInitialState_movesTab() {
        mListener.onScroll(mEventSrc, mEventTo, DISTX, 50.f);
        assertEquals(GestureState.DRAG_TAB, mListener.getStateForTesting());
    }

    @Test
    public void dragDownFromInitialState_scrollsContents() {
        mListener.onScroll(mEventSrc, mEventTo, DISTX, -50.f);
        assertEquals(GestureState.SCROLL_CONTENT, mListener.getStateForTesting());
    }
}
