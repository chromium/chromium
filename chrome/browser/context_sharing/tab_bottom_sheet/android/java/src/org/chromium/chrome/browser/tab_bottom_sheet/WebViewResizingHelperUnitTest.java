// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;

/** Unit tests for {@link WebViewResizingHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebViewResizingHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ThinWebView mMockThinWebView;
    @Mock private WebContents mMockWebContents;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private InsetObserver mMockInsetObserver;
    @Mock private Window mMockWindow;
    @Mock private View mMockDecorView;
    @Captor private ArgumentCaptor<WindowInsetsAnimationListener> mAnimationListenerCaptor;

    private Context mContext;
    private View mView;
    private View mContainerView;
    private WebViewResizingHelper mHelper;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
        mView = new View(mContext);
        when(mMockThinWebView.getView()).thenReturn(mView);
        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mMockThinWebView)
                .runOnNextFrame(any());

        when(mMockWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindowAndroid.getInsetObserver()).thenReturn(mMockInsetObserver);
        when(mMockWindow.getDecorView()).thenReturn(mMockDecorView);
        when(mMockDecorView.getHeight()).thenReturn(1000);

        mContainerView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        mHelper = new WebViewResizingHelper(mContainerView, mMockWindowAndroid, Color.WHITE);
    }

    @Test
    public void testInitialization() {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        verify(mMockInsetObserver).addWindowInsetsAnimationListener(any());
    }

    @Test
    public void testDestroy() {
        mHelper.destroy();
        verify(mMockInsetObserver).removeWindowInsetsAnimationListener(any());
    }

    @Test
    public void testSetThinWebView() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(2, container.getChildCount());
        assertEquals(mView, container.getChildAt(1));

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(Gravity.TOP, layoutParams.gravity);
    }

    @Test
    public void testReset() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mHelper.reset();

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        assertEquals(View.INVISIBLE, container.getChildAt(0).getVisibility());
    }

    @Test
    public void testRequestResize() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        View placeholder = container.getChildAt(0);

        WebViewResizingHelper.ResizeLock lock = mHelper.requestResize();
        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();

        assertEquals(1000, layoutParams.height);
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        lock.unlock();
        assertEquals(1000, layoutParams.height);
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void testDisableResizingMode_WaitsForNextFrame() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        View placeholder = container.getChildAt(0);

        // Override runOnNextFrame behavior to not automatically run.
        doAnswer(invocation -> null).when(mMockThinWebView).runOnNextFrame(any());

        WebViewResizingHelper.ResizeLock lock = mHelper.requestResize();
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        ArgumentCaptor<Runnable> frameCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        lock.unlock();

        // Verify runOnNextFrame was registered.
        verify(mMockThinWebView).runOnNextFrame(frameCallbackCaptor.capture());

        // Run the frame callback and verify view visibility.
        frameCallbackCaptor.getValue().run();
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void testSetThinWebViewMultipleTimes() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(2, container.getChildCount());
        assertEquals(mView, container.getChildAt(1));
    }

    @Test
    public void testSetToFlexibleHeight() {
        View expandedContent = mContainerView.findViewById(R.id.expanded_content_group);
        expandedContent.getLayoutParams().height = 500;

        mHelper.setToFlexibleHeight();
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, expandedContent.getLayoutParams().height);
    }

    @Test
    public void testSetToFixedHeight() {
        View expandedContent = mContainerView.findViewById(R.id.expanded_content_group);

        mHelper.setToFixedHeight(500);
        assertEquals(500, expandedContent.getLayoutParams().height);
    }

    @Test
    public void testUpdateBounds_EarlyReturn() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        clearInvocations(mMockWebContents);
        clearInvocations(mMockThinWebView);

        // Case 1: width == 0
        container.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 0, 100);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());

        // Case 2: height == 0
        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 0);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());

        // Case 3: width == mWebContents.getWidth() && height == mWebContents.getHeight()
        // Use ViewUtils.dpToPx for conversion to match the logic in updateBounds
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));
        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());

        // Case 4: mWebContents.isDestroyed() is true
        when(mMockWebContents.isDestroyed()).thenReturn(true);
        container.measure(
                View.MeasureSpec.makeMeasureSpec(150, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(250, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 150, 250);
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());
    }

    @Test
    public void testUpdateBounds_SetsSize() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);

        clearInvocations(mMockWebContents);
        clearInvocations(mMockThinWebView);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        verify(mMockThinWebView).resizeWebContents(100, 200);
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());
    }

    @Test
    public void testSetThinWebViewOnly() {
        mHelper.setThinWebView(mMockThinWebView, null);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        // Place holder + ThinWebView
        assertEquals(2, container.getChildCount());
        assertEquals(mView, container.getChildAt(1));

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(1000, layoutParams.height);
    }

    @Test
    public void testSetWebContentsOnly() {
        mHelper.setThinWebView(null, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);

        clearInvocations(mMockWebContents);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        verify(mMockWebContents).setSize(100, 200);
    }

    @Test
    public void testReset_nullThinWebView() {
        mHelper.setThinWebView(null, mMockWebContents);
        mHelper.reset();

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        assertEquals(View.INVISIBLE, container.getChildAt(0).getVisibility());
    }

    @Test
    public void testUpdateBounds_InactiveActivity() {
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);

        clearInvocations(mMockWebContents);
        clearInvocations(mMockThinWebView);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());
    }

    @Test
    public void testUpdateBounds_PausedActivity() {
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.PAUSED);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);

        clearInvocations(mMockWebContents);
        clearInvocations(mMockThinWebView);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        verify(mMockThinWebView).resizeWebContents(100, 200);
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());
    }

    @Test
    public void testUpdateBounds_SetsThinWebViewSize_BottomSheet() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockDecorView.getWidth()).thenReturn(1080);
        when(mMockDecorView.getHeight()).thenReturn(1920);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(1080, layoutParams.width);
        assertEquals(1920, layoutParams.height);
    }

    @Test
    public void testUpdateBounds_SidePanel_FallbackSizing() {
        mHelper = new WebViewResizingHelper(mContainerView, mMockWindowAndroid, Color.WHITE, true);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);
        when(mMockDecorView.getHeight()).thenReturn(1000);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 0, 0);

        int expectedWidth =
                ViewUtils.dpToPx(mContext, SidePanelContainerCoordinator.WIDE_SIDE_PANEL_WIDTH_DP);
        int expectedHeight = 1000;

        verify(mMockThinWebView).resizeWebContents(expectedWidth, expectedHeight);
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());
    }

    @Test
    public void testUpdateBounds_SidePanel_FallbackSizing_WebContentsOnly() {
        mHelper = new WebViewResizingHelper(mContainerView, mMockWindowAndroid, Color.WHITE, true);
        mHelper.setThinWebView(null, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);
        when(mMockDecorView.getHeight()).thenReturn(1000);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 0, 0);

        int expectedWidth =
                ViewUtils.dpToPx(mContext, SidePanelContainerCoordinator.WIDE_SIDE_PANEL_WIDTH_DP);
        int expectedHeight = 1000;

        verify(mMockWebContents).setSize(expectedWidth, expectedHeight);
    }

    @Test
    public void testUpdatePlaceholderHeight() {
        mHelper.updatePlaceholderHeight(150);

        FrameLayout resizingContainer = (FrameLayout) mHelper.getResizingContainer();
        View placeholder = resizingContainer.getChildAt(0);
        assertNotNull(placeholder);
        assertEquals(150, placeholder.getLayoutParams().height);
        View content = placeholder.findViewById(R.id.tab_bottom_sheet_resizing_content);
        assertNotNull(content);
    }

    @Test
    public void testUpdateBounds_PausedDuringInsetAnimation() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        WindowInsetsAnimationListener listener = mAnimationListenerCaptor.getValue();

        // Prepare animation pauses updates
        listener.onPrepare(null);

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);
        clearInvocations(mMockThinWebView);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        // Verify resize is NOT called while inset animation is paused
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // Ending animation unpauses and updates bounds
        listener.onEnd(null);
        verify(mMockThinWebView).resizeWebContents(100, 200);
    }

    @Test
    public void testActivityResumed_ForcesResizeAfterInactive() {
        ArgumentCaptor<ActivityStateObserver> observerCaptor =
                ArgumentCaptor.forClass(ActivityStateObserver.class);
        verify(mMockWindowAndroid).addActivityStateObserver(observerCaptor.capture());
        ActivityStateObserver observer = observerCaptor.getValue();
        assertNotNull(observer);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        // 1. Simulate activity becoming inactive.
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);

        // Layout happens while inactive, updateBounds should return early and not resize.
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));
        clearInvocations(mMockThinWebView);
        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // 2. Simulate activity resuming. Even though dimensions match mWebContents,
        // it should force a resize because ignoreCache is true.
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.RESUMED);
        observer.onActivityResumed();

        verify(mMockThinWebView).resizeWebContents(100, 200);
    }
}
