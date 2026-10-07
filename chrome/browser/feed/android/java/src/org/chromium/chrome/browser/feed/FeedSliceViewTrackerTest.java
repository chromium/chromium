// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.leq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link FeedSliceViewTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class FeedSliceViewTrackerTest {
    private static final int PARENT_SIZE = 1000;

    // Mocking dependencies that are always present, but using a real FeedListContentManager.
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock FeedSliceViewTracker.Observer mObserver;
    @Mock LinearLayoutManager mLayoutManager;
    @Mock ListLayoutHelper mLayoutHelper;
    Activity mActivity;
    RecyclerView mParentView;
    FeedListContentManager mContentManager;

    FeedSliceViewTracker mTracker;

    // Child views are used as needed in some tests.
    View mChildA;
    View mChildB;

    boolean mChildAVisibleRunnable1Called;
    boolean mChildAVisibleRunnable2Called;
    boolean mChildAVisibleRunnable3Called;
    boolean mChildBVisibleRunnable1Called;
    boolean mChildBVisibleRunnable2Called;

    @Before
    public void setUp() {
        mContentManager = new FeedListContentManager();
        // The activity's decor view is not attached, so its visible display frame (the viewport)
        // is the display size, which is controlled via @Config qualifiers.
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mParentView = new RecyclerView(mActivity);
        mParentView.setLayoutManager(mLayoutManager);
        mParentView.setRight(PARENT_SIZE);
        mParentView.setBottom(PARENT_SIZE);
        mChildA = new View(mActivity);
        mChildB = new View(mActivity);
        mTracker =
                Mockito.spy(
                        new FeedSliceViewTracker(
                                mParentView,
                                mActivity,
                                mContentManager,
                                mLayoutHelper,
                                /* watchForUserInteractionReliabilityReport= */ true,
                                mObserver));
    }

    @After
    public void tearDown() {
        ShadowSystemClock.reset();
    }

    @Test
    public void testIsItemVisible_JustEnoughnViewport() {
        setViewDimensions(mChildA, 10, 10);
        setChildVisibleRect(mChildA, 0, 0, 10, 7);
        Assert.assertTrue(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    public void testIsItemVisible_NotEnoughnViewport() {
        setViewDimensions(mChildA, 10, 10);
        setChildVisibleRect(mChildA, 0, 0, 10, 6);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    public void testIsItemVisible_ZeroAreaInViewport() {
        setViewDimensions(mChildA, 10, 10);
        setChildVisibleRect(mChildA, 0, 0, 0, 0);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    public void testIsItemVisible_getChildVisibleRectReturnsFalse() {
        setViewDimensions(mChildA, 10, 10);
        setChildOutsideParent(mChildA);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    public void testIsItemVisible_ZeroArea() {
        setViewDimensions(mChildA, 0, 0);
        setChildVisibleRect(mChildA, 0, 0, 0, 0);
        Assert.assertFalse(mTracker.isViewVisible(mChildA, 0.66f));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
    public void testIsItemCoveringViewport_JustEnough() {
        setViewDimensions(mChildA, 100, 100);
        setChildVisibleRect(mChildA, 0, 0, 100, 26);
        Assert.assertTrue(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
    public void testIsViewCoveringViewport_NotEnough() {
        setViewDimensions(mChildA, 100, 100);
        setChildVisibleRect(mChildA, 0, 0, 100, 24);
        Assert.assertFalse(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
    public void testIsContentCoveringViewport_ZeroArea() {
        setViewDimensions(mChildA, 0, 0);
        setChildVisibleRect(mChildA, 0, 0, 0, 0);
        Assert.assertFalse(mTracker.isViewCoveringViewport(mChildA, 0.25f));
    }

    @Test
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
    public void testDestroy() {
        mTracker.bind();
        mParentView.getViewTreeObserver().dispatchOnPreDraw();
        verify(mTracker).onPreDraw();
        clearInvocations(mTracker);

        mTracker.destroy();
        // Ensure onPreDraw not called again after destroy().
        mParentView.getViewTreeObserver().dispatchOnPreDraw();
        verify(mTracker, never()).onPreDraw();

        // These calls shouldn't do anything.
        mTracker.destroy();
        mTracker.clear();
        mTracker.watchForFirstVisible("c/key1", 0.5f, () -> {});
        mTracker.stopWatchingForFirstVisible("c/key1", () -> {});
    }

    @Test
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
        Runnable childBVisibleRunnable1 =
                () -> {
                    mChildBVisibleRunnable1Called = true;
                };
        mTracker.watchForFirstVisible("c/key2", 0.6f, childBVisibleRunnable1);
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
        mTracker.stopWatchingForFirstVisible("c/key2", childBVisibleRunnable1);
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
    @Config(qualifiers = "w100dp-h100dp")
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
        setViewDimensions(mChildA, 100, 15);
        setChildVisibleRect(mChildA, 0, 0, 100, 15);
        setViewDimensions(mChildB, 100, 15);
        setChildVisibleRect(mChildB, 0, 15, 100, 30);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
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
        setViewDimensions(mChildA, 100, 26);
        setChildVisibleRect(mChildA, 0, 0, 100, 26);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
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
        setViewDimensions(mChildA, 100, 22);
        setChildVisibleRect(mChildA, 0, 0, 100, 22);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
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
        setViewDimensions(mChildA, 100, 22);
        setChildVisibleRect(mChildA, 0, 0, 100, 22);

        mTracker.onPreDraw();
        advanceByMs(1L);
        mTracker.unbind();
        verify(mObserver, times(1)).reportContentSliceVisibleTime(eq(1L));
    }

    @Test
    @Config(qualifiers = "w100dp-h100dp")
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
        setViewDimensions(mChildA, 100, 5);
        setChildVisibleRect(mChildA, 0, 0, 100, 5);

        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportViewFirstBarelyVisible(any());
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mObserver, times(1)).reportViewFirstRendered(any());
    }

    @Test
    @Config(qualifiers = "w500dp-h500dp")
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

        setViewDimensions(mChildA, 100, 100);
        setViewDimensions(mChildB, 100, 100);
        setChildOutsideParent(mChildB);

        // No report when less than 5% visible.
        setChildVisibleRect(mChildA, 0, 0, 100, 4);
        mTracker.onPreDraw();
        verify(mObserver, times(0)).reportLoadMoreIndicatorVisible();

        // Report when 5% visible.
        setChildVisibleRect(mChildA, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();

        // No more report when more visible.
        setChildVisibleRect(mChildA, 0, 0, 100, 10);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();

        // Report for another indicator.
        setChildVisibleRect(mChildB, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(2)).reportLoadMoreIndicatorVisible();
    }

    @Test
    @Config(qualifiers = "w500dp-h500dp")
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

        setViewDimensions(mChildA, 100, 100);
        setViewDimensions(mChildB, 100, 100);
        setChildOutsideParent(mChildB);

        // Report visible when 5% visible.
        setChildVisibleRect(mChildA, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(0)).reportLoadMoreUserScrolledAwayFromIndicator();

        // Report away when not visible.
        setChildVisibleRect(mChildA, 0, 0, 100, 0);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(1)).reportLoadMoreUserScrolledAwayFromIndicator();

        // No more report when further away.
        setChildVisibleRect(mChildA, 0, 0, 100, -10);
        mTracker.onPreDraw();
        verify(mObserver, times(1)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(1)).reportLoadMoreUserScrolledAwayFromIndicator();

        // Report for another indicator.
        setChildVisibleRect(mChildB, 0, 0, 100, 5);
        mTracker.onPreDraw();
        verify(mObserver, times(2)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(1)).reportLoadMoreUserScrolledAwayFromIndicator();

        setChildVisibleRect(mChildB, 0, 0, 100, 0);
        mTracker.onPreDraw();
        verify(mObserver, times(2)).reportLoadMoreIndicatorVisible();
        verify(mObserver, times(2)).reportLoadMoreUserScrolledAwayFromIndicator();
    }

    void setViewDimensions(View view, int width, int height) {
        view.layout(0, 0, width, height);
    }

    /**
     * Positions {@code child} (whose size was set via {@link #setViewDimensions}) within the parent
     * such that the part of it that is inside the parent is the given rect. The child is clipped by
     * the parent's top / left edges as needed.
     */
    void setChildVisibleRect(View child, int rectLeft, int rectTop, int rectRight, int rectBottom) {
        int width = child.getWidth();
        int height = child.getHeight();
        child.layout(rectRight - width, rectBottom - height, rectRight, rectBottom);
        assertEquals(rectLeft, Math.max(child.getLeft(), 0));
        assertEquals(rectTop, Math.max(child.getTop(), 0));
    }

    /** Positions {@code child} entirely outside of the parent. */
    void setChildOutsideParent(View child) {
        int width = child.getWidth();
        int height = child.getHeight();
        child.layout(PARENT_SIZE, PARENT_SIZE, PARENT_SIZE + width, PARENT_SIZE + height);
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
