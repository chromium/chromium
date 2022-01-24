// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.leq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyFloat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;

/** Unit tests for {@link FeedSliceViewTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedSliceViewTrackerTest {
    // Mocking dependencies that are always present, but using a real FeedListContentManager.
    @Mock
    RecyclerView mParentView;
    @Mock
    FeedSliceViewTracker.Observer mObserver;
    @Mock
    LinearLayoutManager mLayoutManager;
    @Mock
    ViewTreeObserver mViewTreeObserver;
    NtpListContentManager mContentManager;

    FeedSliceViewTracker mTracker;

    // Child view mocks are used as needed in some tests.
    @Mock
    View mChildA;
    @Mock
    View mChildB;

    boolean mChildAVisibleRunnable1Called;
    boolean mChildAVisibleRunnable2Called;
    boolean mChildAVisibleRunnable3Called;
    boolean mChildBVisibleRunnable1Called;
    boolean mChildBVisibleRunnable2Called;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        mContentManager = new NtpListContentManager();
        doReturn(mLayoutManager).when(mParentView).getLayoutManager();
        doReturn(mViewTreeObserver).when(mParentView).getViewTreeObserver();
        mTracker = Mockito.spy(new FeedSliceViewTracker(mParentView, mContentManager, mObserver));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_JustEnoughnViewport() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRect(mChildA, 0, 7);
        Assert.assertTrue(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_NotEnoughnViewport() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRect(mChildA, 0, 6);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_ZeroAreaInViewport() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRect(mChildA, 0, 0);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_getChildVisibleRectReturnsFalse() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRectIsEmpty(mChildA);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_ZeroArea() {
        mockViewDimensions(mChildA, 0, 0);
        mockGetChildVisibleRect(mChildA, 0, 0);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testOnPreDraw_BothVisibleAreReportedExactlyOnce() {
        mContentManager.addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        new NtpListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        new NtpListContentManager.NativeViewContent(0, "c/key2", mChildB),
                }));
        doReturn(0).when(mLayoutManager).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutManager).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(true).when(mTracker).isViewVisible(eq(mChildB), anyFloat());

        mTracker.onPreDraw();

        verify(mObserver).feedContentVisible();
        verify(mObserver).sliceVisible(eq("c/key1"));
        verify(mObserver).sliceVisible(eq("c/key2"));

        mTracker.onPreDraw(); // Does not repeat call to sliceVisible().
    }

    @Test
    @SmallTest
    public void testOnPreDraw_AfterClearReportsAgain() {
        mContentManager.addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        new NtpListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        new NtpListContentManager.NativeViewContent(0, "c/key2", mChildB),
                }));
        doReturn(0).when(mLayoutManager).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutManager).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(true).when(mTracker).isViewVisible(eq(mChildB), anyFloat());

        mTracker.onPreDraw();
        mTracker.clear();
        mTracker.onPreDraw(); // repeats observer calls.

        verify(mObserver, times(2)).feedContentVisible();
        verify(mObserver, times(2)).sliceVisible(eq("c/key1"));
        verify(mObserver, times(2)).sliceVisible(eq("c/key2"));
    }

    @Test
    @SmallTest
    public void testOnPreDraw_IgnoresNonContentViews() {
        mContentManager.addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        new NtpListContentManager.NativeViewContent(0, "non-content-key1", mChildA),
                        new NtpListContentManager.NativeViewContent(0, "non-content-key2", mChildB),
                }));
        doReturn(0).when(mLayoutManager).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutManager).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(true).when(mTracker).isViewVisible(eq(mChildB), anyFloat());

        mTracker.onPreDraw();

        verify(mObserver, times(0)).feedContentVisible();
        verify(mObserver, times(0)).sliceVisible(any());

        mTracker.onPreDraw(); // Does not repeat call to sliceVisible().
    }

    @Test
    @SmallTest
    public void testOnPreDraw_OnlyOneVisible() {
        mContentManager.addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        new NtpListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        new NtpListContentManager.NativeViewContent(0, "c/key2", mChildB),
                }));
        doReturn(0).when(mLayoutManager).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutManager).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        doReturn(false).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(true).when(mTracker).isViewVisible(eq(mChildB), anyFloat());

        mTracker.onPreDraw();

        verify(mObserver).sliceVisible(eq("c/key2"));
    }

    @Test
    @SmallTest
    public void testOnPreDraw_EmptyRecyclerView() {
        mContentManager.addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        new NtpListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        new NtpListContentManager.NativeViewContent(0, "c/key2", mChildB),
                }));
        doReturn(RecyclerView.NO_POSITION).when(mLayoutManager).findFirstVisibleItemPosition();
        doReturn(RecyclerView.NO_POSITION).when(mLayoutManager).findLastVisibleItemPosition();

        mTracker.onPreDraw();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        doReturn(true).when(mViewTreeObserver).isAlive();
        mTracker.destroy();
        verify(mViewTreeObserver).removeOnPreDrawListener(any());

        mTracker.destroy(); // A second destroy() does nothing.
    }

    @Test
    @SmallTest
    public void testWatchForFirstVisible() {
        mContentManager.addContents(0,
                Arrays.asList(new NtpListContentManager.FeedContent[] {
                        new NtpListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        new NtpListContentManager.NativeViewContent(0, "c/key2", mChildB),
                }));
        doReturn(0).when(mLayoutManager).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutManager).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        // Associates 3 observers with one content key.
        mTracker.watchForFirstVisible(
                "c/key1", 0.5f, () -> { mChildAVisibleRunnable1Called = true; });
        mTracker.watchForFirstVisible(
                "c/key1", 0.7f, () -> { mChildAVisibleRunnable2Called = true; });
        mTracker.watchForFirstVisible(
                "c/key1", 0.4f, () -> { mChildAVisibleRunnable3Called = true; });

        // Associates 2 observers with another content key.
        Runnable mChildBVisibleRunnable1 = () -> {
            mChildBVisibleRunnable1Called = true;
        };
        mTracker.watchForFirstVisible("c/key2", 0.6f, mChildBVisibleRunnable1);
        mTracker.watchForFirstVisible(
                "c/key2", 0.7f, () -> { mChildBVisibleRunnable2Called = true; });

        // Expects that 2 observers associated with same content key get invoked.
        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), leq(0.5f));
        doReturn(false).when(mTracker).isViewVisible(eq(mChildB), leq(0.5f));
        clearVisibleRunnableCalledStates();
        mTracker.onPreDraw();
        assertTrue(mChildAVisibleRunnable1Called);
        assertFalse(mChildAVisibleRunnable2Called);
        assertTrue(mChildAVisibleRunnable3Called);
        assertFalse(mChildBVisibleRunnable1Called);
        assertFalse(mChildBVisibleRunnable2Called);

        // Raises the threshold. Exepcts that 2 observers notified last time will not get notified
        // this time, while another observer is notified due to the raised threshold.
        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), leq(0.7f));
        clearVisibleRunnableCalledStates();
        mTracker.onPreDraw();
        assertFalse(mChildAVisibleRunnable1Called);
        assertTrue(mChildAVisibleRunnable2Called);
        assertFalse(mChildAVisibleRunnable3Called);
        assertFalse(mChildBVisibleRunnable1Called);
        assertFalse(mChildBVisibleRunnable2Called);

        // Stops watching an observer. Expects that this observe will not get notified.
        mTracker.stopWatchingForFirstVisible("c/key2", mChildBVisibleRunnable1);
        doReturn(true).when(mTracker).isViewVisible(eq(mChildB), leq(0.7f));
        clearVisibleRunnableCalledStates();
        mTracker.onPreDraw();
        assertFalse(mChildAVisibleRunnable1Called);
        assertFalse(mChildAVisibleRunnable2Called);
        assertFalse(mChildAVisibleRunnable3Called);
        assertFalse(mChildBVisibleRunnable1Called);
        assertTrue(mChildBVisibleRunnable2Called);
    }

    void mockViewDimensions(View view, int width, int height) {
        when(view.getWidth()).thenReturn(10);
        when(view.getHeight()).thenReturn(10);
    }

    void mockGetChildVisibleRect(View child, int rectTop, int rectBottom) {
        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) {
                Rect rect = (Rect) invocation.getArguments()[1];
                rect.top = rectTop;
                rect.bottom = rectBottom;
                return true;
            }
        })
                .when(mParentView)
                .getChildVisibleRect(eq(child), any(), any());
    }

    void mockGetChildVisibleRectIsEmpty(View child) {
        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) {
                return false;
            }
        })
                .when(mParentView)
                .getChildVisibleRect(eq(child), any(), any());
    }

    void clearVisibleRunnableCalledStates() {
        mChildAVisibleRunnable1Called = false;
        mChildAVisibleRunnable2Called = false;
        mChildAVisibleRunnable3Called = false;
        mChildBVisibleRunnable1Called = false;
        mChildBVisibleRunnable2Called = false;
    }
}
