// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.leq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Looper;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link FeedSliceViewTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class FeedSliceViewTrackerTest {
    // Mocking dependencies that are always present, but using a real FeedListContentManager.
    @Mock RecyclerView mParentView;
    @Mock FeedSliceViewTracker.Observer mObserver;
    @Mock LinearLayoutManager mLayoutManager;
    @Mock ListLayoutHelper mLayoutHelper;
    @Mock ViewTreeObserver mViewTreeObserver;
    @Mock Activity mActivity;
    @Mock Window mWindow;
    @Mock View mDecorView;
    FeedListContentManager mContentManager;

    FeedSliceViewTracker mTracker;

    // Child view mocks are used as needed in some tests.
    @Mock View mChildA;
    @Mock View mChildB;

    boolean mChildAVisibleRunnable1Called;
    boolean mChildAVisibleRunnable2Called;
    boolean mChildAVisibleRunnable3Called;
    boolean mChildBVisibleRunnable1Called;
    boolean mChildBVisibleRunnable2Called;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        mContentManager = new FeedListContentManager();
        doReturn(mLayoutManager).when(mParentView).getLayoutManager();
        doReturn(mViewTreeObserver).when(mParentView).getViewTreeObserver();
        doReturn(mWindow).when(mActivity).getWindow();
        doReturn(mDecorView).when(mWindow).getDecorView();
        mTracker =
                Mockito.spy(
                        new FeedSliceViewTracker(
                                mParentView,
                                mActivity,
                                mContentManager,
                                mLayoutHelper,
                                /* mWatchForUserInteractionReliabilityReport= */ true,
                                mObserver));
    }

    @After
    public void tearDown() {
        ShadowSystemClock.reset();
    }

    @Test
    @SmallTest
    public void testIsItemVisible_JustEnoughnViewport() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRect(mChildA, 0, 0, 10, 7);
        Assert.assertTrue(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_NotEnoughnViewport() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRect(mChildA, 0, 0, 10, 6);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testIsItemVisible_ZeroAreaInViewport() {
        mockViewDimensions(mChildA, 10, 10);
        mockGetChildVisibleRect(mChildA, 0, 0, 0, 0);
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
        mockGetChildVisibleRect(mChildA, 0, 0, 0, 0);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @SmallTest
    public void testGetChildVisibleRectCalledWithChildRect() {
        mockViewDimensions(mChildA, 10, 10);
        mTracker.isViewVisible(mChildA, 0.66f);
        verify(mParentView).getChildVisibleRect(eq(mChildA), eq(new Rect(0, 0, 10, 10)), eq(null));
    }

    @Test
    @SmallTest
    public void testIsItemCoveringViewport_JustEnough() {
        mockViewDimensions(mChildA, 100, 100);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 26);
        mockViewportRect(0, 0, 100, 100);
        Assert.assertTrue(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
    @SmallTest
    public void testIsViewCoveringViewport_NotEnough() {
        mockViewDimensions(mChildA, 100, 100);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 24);
        mockViewportRect(0, 0, 100, 100);
        Assert.assertFalse(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
    @SmallTest
    public void testIsContentCoveringViewport_ZeroArea() {
        mockViewDimensions(mChildA, 0, 0);
        mockGetChildVisibleRect(mChildA, 0, 0, 0, 0);
        mockViewportRect(0, 0, 100, 100);
        Assert.assertFalse(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
    @SmallTest
    public void testIsContentCoveringViewport_NoViewport() {
        mockViewDimensions(mChildA, 100, 100);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 26);
        mockViewportRect(0, 0, 0, 0);
        Assert.assertFalse(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
    @SmallTest
    public void testOnPreDraw_BothVisibleAreReportedExactlyOnce() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
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
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
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
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(
                                    0, "non-content-key1", mChildA),
                            new FeedListContentManager.NativeViewContent(
                                    0, "non-content-key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
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
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
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
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(RecyclerView.NO_POSITION).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(RecyclerView.NO_POSITION).when(mLayoutHelper).findLastVisibleItemPosition();

        mTracker.onPreDraw();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        doReturn(true).when(mViewTreeObserver).isAlive();
        mTracker.destroy();
        verify(mViewTreeObserver).removeOnPreDrawListener(any());

        // These calls shouldn't do anything.
        mTracker.destroy();
        mTracker.clear();
        mTracker.watchForFirstVisible("c/key1", 0.5f, () -> {});
        mTracker.stopWatchingForFirstVisible("c/key1", () -> {});
    }

    @Test
    @SmallTest
    public void testWatchForFirstVisible() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        // Associates 3 observers with one content key.
        mTracker.watchForFirstVisible(
                "c/key1",
                0.5f,
                () -> {
                    mChildAVisibleRunnable1Called = true;
                });
        mTracker.watchForFirstVisible(
                "c/key1",
                0.7f,
                () -> {
                    mChildAVisibleRunnable2Called = true;
                });
        mTracker.watchForFirstVisible(
                "c/key1",
                0.4f,
                () -> {
                    mChildAVisibleRunnable3Called = true;
                });

        // Associates 2 observers with another content key.
        Runnable mChildBVisibleRunnable1 =
                () -> {
                    mChildBVisibleRunnable1Called = true;
                };
        mTracker.watchForFirstVisible("c/key2", 0.6f, mChildBVisibleRunnable1);
        mTracker.watchForFirstVisible(
                "c/key2",
                0.7f,
                () -> {
                    mChildBVisibleRunnable2Called = true;
                });

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

    @Test
    @SmallTest
    public void testReportContentVisibleTime_visibleAndCovering() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        // Not visible or covering: no time reported.
        doReturn(false).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(false).when(mTracker).isViewCoveringViewport(eq(mChildA), anyFloat());
        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, never()).reportContentSliceVisibleTime(anyLong());

        // Visible enough; time is reported.
        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(false).when(mTracker).isViewCoveringViewport(eq(mChildA), anyFloat());
        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
        reset(mObserver);

        // Covering enough; time is reported.
        doReturn(false).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(true).when(mTracker).isViewCoveringViewport(eq(mChildA), anyFloat());
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
        reset(mObserver);

        // Visible enough and covering enough: report some time spent in feed.
        doReturn(true).when(mTracker).isViewVisible(eq(mChildA), anyFloat());
        doReturn(true).when(mTracker).isViewCoveringViewport(eq(mChildA), anyFloat());
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @SmallTest
    public void testReportContentVisibleTime_testSmallCardsCoveringEnough() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                            new FeedListContentManager.NativeViewContent(0, "c/key2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        // Views are completely exposed so time is tracked.
        mockViewportRect(0, 0, 100, 100);
        mockViewDimensions(mChildA, 100, 15);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 15);
        mockViewDimensions(mChildB, 100, 15);
        mockGetChildVisibleRect(mChildB, 0, 15, 100, 30);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @SmallTest
    public void testReportContentVisibleTime_testBigCardCoveringEnough() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(0).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));

        // View is completely exposed and covers 30% of the viewport in total.
        mockViewportRect(0, 0, 100, 100);
        mockViewDimensions(mChildA, 100, 26);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 26);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @SmallTest
    public void testReportContentVisibleTime_testBigCardExposedEnough() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(0).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));

        // View is completely exposed but only covers 22% of the viewport.
        mockViewportRect(0, 0, 100, 100);
        mockViewDimensions(mChildA, 100, 22);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 22);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @SmallTest
    public void testReportContentVisibleTime_testReportTimeOnUnbind() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(0).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));

        // View is completely exposed but only covers 22% of the viewport.
        mockViewportRect(0, 0, 100, 100);
        mockViewDimensions(mChildA, 100, 22);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 22);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.unbind();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @SmallTest
    public void testReportViewFirstVisibleAndRendered() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(0, "c/key1", mChildA),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(0).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));

        // View only covers 5% of the viewport.
        mockViewportRect(0, 0, 100, 100);
        mockViewDimensions(mChildA, 100, 5);
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 5);

        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportViewFirstBarelyVisible(any());
        shadowOf(Looper.getMainLooper()).idle();
        verify(mObserver, times(1)).reportViewFirstRendered(any());
    }

    @Test
    @SmallTest
    public void testReportLoadMoreIndicatorVisible() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(
                                    0, "load-more-spinner1", mChildA),
                            new FeedListContentManager.NativeViewContent(
                                    1, "load-more-spinner2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        mockViewportRect(0, 0, 500, 500);
        mockViewDimensions(mChildA, 100, 100);
        mockViewDimensions(mChildB, 100, 100);

        // No report when less than 5% visible.
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 4);
        mTracker.onPreDraw();
        verify(mObserver, times(0)).reportLoadMoreIndicatorVisible();

        // Report when 5% visible.
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();

        // No more report when more visible.
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 10);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();

        // Report for another indicator.
        mockGetChildVisibleRect(mChildB, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(2)).reportLoadMoreIndicatorVisible();
    }

    @Test
    @SmallTest
    public void testReportLoadMoreAwayFromIndicator() {
        mContentManager.addContents(
                0,
                Arrays.asList(
                        new FeedListContentManager.FeedContent[] {
                            new FeedListContentManager.NativeViewContent(
                                    0, "load-more-spinner1", mChildA),
                            new FeedListContentManager.NativeViewContent(
                                    1, "load-more-spinner2", mChildB),
                        }));
        doReturn(0).when(mLayoutHelper).findFirstVisibleItemPosition();
        doReturn(1).when(mLayoutHelper).findLastVisibleItemPosition();
        doReturn(mChildA).when(mLayoutManager).findViewByPosition(eq(0));
        doReturn(mChildB).when(mLayoutManager).findViewByPosition(eq(1));

        mockViewportRect(0, 0, 500, 500);
        mockViewDimensions(mChildA, 100, 100);
        mockViewDimensions(mChildB, 100, 100);

        // Report visible when 5% visible.
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(0)).reportLoadMoreUserScrolledAwayFromIndicator();

        // Report away when not visible.
        mockGetChildVisibleRect(mChildA, 0, 0, 100, 0);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(1)).reportLoadMoreUserScrolledAwayFromIndicator();

        // No more report when further away.
        mockGetChildVisibleRect(mChildA, 0, 0, 100, -10);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(1)).reportLoadMoreUserScrolledAwayFromIndicator();

        // Report for another indicator.
        mockGetChildVisibleRect(mChildB, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(2)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(1)).reportLoadMoreUserScrolledAwayFromIndicator();

        mockGetChildVisibleRect(mChildB, 0, 0, 100, 0);
        mTracker.onPreDraw();
        verify(mObserver, times(2)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(2)).reportLoadMoreUserScrolledAwayFromIndicator();
    }

    void mockViewDimensions(View view, int width, int height) {
        when(view.getWidth()).thenReturn(width);
        when(view.getHeight()).thenReturn(height);
    }

    void mockGetChildVisibleRect(
            View child, int rectLeft, int rectTop, int rectRight, int rectBottom) {
        doAnswer(
                        new Answer() {
                            @Override
                            public Object answer(InvocationOnMock invocation) {
                                Rect rect = (Rect) invocation.getArguments()[1];
                                rect.top = rectTop;
                                rect.bottom = rectBottom;
                                rect.left = rectLeft;
                                rect.right = rectRight;
                                return true;
                            }
                        })
                .when(mParentView)
                .getChildVisibleRect(eq(child), any(), any());
    }

    void mockGetChildVisibleRectIsEmpty(View child) {
        doAnswer(
                        new Answer() {
                            @Override
                            public Object answer(InvocationOnMock invocation) {
                                return false;
                            }
                        })
                .when(mParentView)
                .getChildVisibleRect(eq(child), any(), any());
    }

    void mockViewportRect(int left, int top, int right, int bottom) {
        doAnswer(
                        new Answer() {
                            @Override
                            public Object answer(InvocationOnMock invocation) {
                                ((Rect) invocation.getArguments()[0])
                                        .set(new Rect(left, top, right, bottom));
                                return null;
                            }
                        })
                .when(mDecorView)
                .getWindowVisibleDisplayFrame(any());
    }

    void clearVisibleRunnableCalledStates() {
        mChildAVisibleRunnable1Called = false;
        mChildAVisibleRunnable2Called = false;
        mChildAVisibleRunnable3Called = false;
        mChildBVisibleRunnable1Called = false;
        mChildBVisibleRunnable2Called = false;
    }

    void advanceByMs(long ms) {
        ShadowSystemClock.advanceBy(ms, TimeUnit.MILLISECONDS);
    }
}
