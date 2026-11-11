// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_bottom_sheet.WebViewResizingHelper.ResizeLock;
import org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link WebViewResizingHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w1080dp-h2000dp")
public class WebViewResizingHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ThinWebView mMockThinWebView;
    @Mock private WebContents mMockWebContents;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private BottomSheetContent mMockSheetContent;
    @Mock private DisplayAndroid mMockDisplayAndroid;
    @Mock private InsetObserver mMockInsetObserver;
    @Mock private Window mMockWindow;
    @Mock private CoBrowseComponentProvider mMockComponentProvider;
    @Captor private ArgumentCaptor<WindowInsetsAnimationListener> mAnimationListenerCaptor;

    private TestActivity mActivity;
    private Context mContext;
    private View mMockDecorView;
    private View mView;
    private View mContainerView;
    private @SheetState int mCurrentSheetState;
    private int mTargetImeInsetPx;
    private WebViewResizingHelper mHelper;
    private final SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier =
            ObservableSuppliers.createNonNull(0);

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mContext = activity;
                        });
        lenient().when(mMockBottomSheetController.hasBottomInset()).thenReturn(true);
        mView = new View(mContext);
        mView.setFocusable(true);
        mView.setFocusableInTouchMode(true);
        when(mMockThinWebView.getView()).thenReturn(mView);
        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mMockThinWebView)
                .runOnNextFrame(any());

        mMockDecorView = new View(mContext);
        mMockDecorView.layout(0, 0, 0, 1000);
        when(mMockWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindowAndroid.getDisplay()).thenReturn(mMockDisplayAndroid);
        when(mMockDisplayAndroid.getDipScale()).thenReturn(1.0f);
        when(mMockWindowAndroid.getInsetObserver()).thenReturn(mMockInsetObserver);
        when(mMockInsetObserver.getSupplierForKeyboardInset()).thenReturn(mKeyboardInsetSupplier);
        when(mMockWindow.getDecorView()).thenReturn(mMockDecorView);
        when(mMockComponentProvider.createResizingPlaceholderCoordinator(any(), anyInt(), anyInt()))
                .thenAnswer(
                        inv ->
                                new LegacyResizingPlaceholderCoordinator(
                                        inv.getArgument(0), inv.getArgument(1)));

        mContainerView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        mHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ false,
                        mMockComponentProvider);
    }

    @Test
    public void testInitialization() {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(0, container.getChildCount());
        verify(mMockInsetObserver).addWindowInsetsAnimationListener(any());
        assertEquals(mMockComponentProvider, mHelper.getComponentProviderForTesting());
        verify(mMockComponentProvider, never())
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        assertNull(mHelper.getPlaceholderCoordinator());
    }

    @Test
    public void testComponentProvider_PassedThroughConstructor() {
        CoBrowseComponentProvider mockProvider = mock(CoBrowseComponentProvider.class);
        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        false,
                        mockProvider);
        assertEquals(mockProvider, helper.getComponentProviderForTesting());
    }

    @Test
    public void testComponentProvider_NullPassedThroughConstructor() {
        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ false,
                        /* componentProvider= */ null);
        assertNull(helper.getComponentProviderForTesting());
    }

    @Test
    public void testDestroy() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        ResizingPlaceholderCoordinator placeholderCoordinator = mHelper.getPlaceholderCoordinator();
        assertNotNull(placeholderCoordinator);
        View placeholder = placeholderCoordinator.getView();
        placeholder.setVisibility(View.VISIBLE);
        placeholder.setAlpha(0.4f);

        mHelper.destroy();

        verify(mMockInsetObserver).removeWindowInsetsAnimationListener(any());
        verify(mMockWindowAndroid).removeActivityStateObserver(any());
        assertEquals(View.INVISIBLE, placeholder.getVisibility());
        assertEquals(1.0f, placeholder.getAlpha(), 0.01f);
        assertEquals(0, ((FrameLayout) mHelper.getResizingContainer()).getChildCount());
    }

    @Test
    public void testSetThinWebView() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        assertEquals(mView, container.getChildAt(0));

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(Gravity.TOP, layoutParams.gravity);
    }

    @Test
    public void testReset() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        View placeholder = mHelper.getPlaceholderCoordinator().getView();
        placeholder.setVisibility(View.VISIBLE);
        placeholder.setAlpha(0.4f);
        mHelper.reset();

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(1, container.getChildCount());
        assertEquals(View.INVISIBLE, container.getChildAt(0).getVisibility());
        assertEquals(1.0f, container.getChildAt(0).getAlpha(), 0.01f);
    }

    @Test
    public void testLazyPlaceholderInflation_InflatesOnResizeAtZIndexZero() {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(0, container.getChildCount());
        verify(mMockComponentProvider, never())
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());

        // Updating placeholder height before resize caches the height without inflating.
        mHelper.updatePlaceholderHeight(350);
        verify(mMockComponentProvider, never())
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        assertEquals(0, container.getChildCount());

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        assertEquals(1, container.getChildCount());
        assertSame(mView, container.getChildAt(0));
        verify(mMockComponentProvider, never())
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        verify(mMockComponentProvider)
                .createResizingPlaceholderCoordinator(mContext, Color.WHITE, Color.LTGRAY);
        assertEquals(2, container.getChildCount());
        assertSame(mHelper.getPlaceholderCoordinator().getView(), container.getChildAt(0));
        assertSame(mView, container.getChildAt(1));

        lock.unlock();
    }

    @Test
    public void testRequestResize() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(mHelper.getPlaceholderCoordinator());
        View placeholder = mHelper.getPlaceholderCoordinator().getView();
        assertSame(placeholder, container.getChildAt(0));

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

        // Override runOnNextFrame behavior to not automatically run.
        doAnswer(invocation -> null).when(mMockThinWebView).runOnNextFrame(any());

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(mHelper.getPlaceholderCoordinator());
        View placeholder = mHelper.getPlaceholderCoordinator().getView();
        assertSame(placeholder, container.getChildAt(0));
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
        assertEquals(1, container.getChildCount());
        assertEquals(mView, container.getChildAt(0));
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
        assertEquals(1, container.getChildCount());
        assertEquals(mView, container.getChildAt(0));

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) mView.getLayoutParams();
        assertEquals(1000, layoutParams.height);
    }

    @Test
    public void testSetWebContentsOnly() {
        mHelper.setThinWebView(null, mMockWebContents);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        assertEquals(0, container.getChildCount());

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
        assertEquals(0, container.getChildCount());
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

        mMockDecorView.layout(0, 0, 1080, 1920);

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
        mHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        true,
                        mMockComponentProvider);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);
        mMockDecorView.layout(0, 0, 0, 1000);

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
        mHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        true,
                        mMockComponentProvider);
        mHelper.setThinWebView(null, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(50);
        when(mMockWebContents.getHeight()).thenReturn(50);
        mMockDecorView.layout(0, 0, 0, 1000);

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
    public void testUpdateBounds_SidePanel_ZeroDecorHeight_EarlyReturn() {
        mMockDecorView.layout(0, 0, 0, 0);
        mHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        true,
                        mMockComponentProvider);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        clearInvocations(mMockWebContents);
        clearInvocations(mMockThinWebView);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 0, 0);

        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
        verify(mMockWebContents, never()).setSize(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testUpdatePlaceholderHeight_ClampsNegativeAndAppliesInsetsBeforeHeight() {
        CoBrowseComponentProvider provider = mock(CoBrowseComponentProvider.class);
        ResizingPlaceholderCoordinator customCoordinator =
                mock(ResizingPlaceholderCoordinator.class);
        when(provider.createResizingPlaceholderCoordinator(any(), anyInt(), anyInt()))
                .thenReturn(customCoordinator);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);

        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ false,
                        provider);

        // Positive height cached first.
        helper.updatePlaceholderHeight(250);
        // Overwritten by negative height which clamps to 0.
        helper.updatePlaceholderHeight(-50);

        helper.setThinWebView(mMockThinWebView, mMockWebContents);
        verify(provider, never()).createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());

        ResizeLock lock = helper.requestResize();
        assertNotNull(lock);
        verify(provider).createResizingPlaceholderCoordinator(mContext, Color.WHITE, Color.LTGRAY);

        // Verify bottom inset is updated before visible height so skeleton layout has updated
        // padding.
        InOrder inOrder = inOrder(customCoordinator);
        inOrder.verify(customCoordinator).updateBottomInset(48);
        inOrder.verify(customCoordinator).updateVisibleHeight(0);

        lock.unlock();
        helper.destroy();
    }

    @Test
    public void testDestroy_BeforeResizeDoesNotInflatePlaceholder() {
        CoBrowseComponentProvider provider = mock(CoBrowseComponentProvider.class);
        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ false,
                        provider);

        helper.setThinWebView(mMockThinWebView, mMockWebContents);
        helper.reset();
        helper.destroy();

        verify(provider, never()).createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        assertNull(helper.getPlaceholderCoordinator());
    }

    @Test
    public void testUpdatePlaceholderHeight_LimitsAndAlphaTransitions() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        FrameLayout resizingContainer = (FrameLayout) mHelper.getResizingContainer();
        View placeholder = resizingContainer.getChildAt(0);
        View content = placeholder.findViewById(R.id.tab_bottom_sheet_resizing_content);

        // Measure content to a known height of 60px.
        content.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(60, View.MeasureSpec.EXACTLY));
        content.layout(0, 0, 100, 60);

        int minHeight =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total);
        int fadeOffset =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_resizing_fade_offset);
        int maxHeight = 60 + fadeOffset;

        // 1. visibleHeight <= minHeight -> alpha = 0.0f
        mHelper.updatePlaceholderHeight(minHeight - 10);
        assertEquals(0.0f, content.getAlpha(), 0.01f);
        assertEquals(minHeight - 10, placeholder.getLayoutParams().height);

        mHelper.updatePlaceholderHeight(minHeight);
        assertEquals(0.0f, content.getAlpha(), 0.01f);

        // 2. visibleHeight between minHeight and maxHeight -> interpolated alpha
        int midHeight = (minHeight + maxHeight) / 2;
        mHelper.updatePlaceholderHeight(midHeight);
        float expectedMidAlpha = (float) (midHeight - minHeight) / (maxHeight - minHeight);
        assertEquals(expectedMidAlpha, content.getAlpha(), 0.01f);

        // 3. visibleHeight >= maxHeight -> alpha = 1.0f
        mHelper.updatePlaceholderHeight(maxHeight);
        assertEquals(1.0f, content.getAlpha(), 0.01f);

        mHelper.updatePlaceholderHeight(maxHeight + 50);
        assertEquals(1.0f, content.getAlpha(), 0.01f);
        assertEquals(maxHeight + 50, placeholder.getLayoutParams().height);

        // 4. Edge case: maxHeight <= minHeight (e.g. content height is 0)
        content.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY));
        content.layout(0, 0, 100, 0);
        // fadeOffset (40dp) <= minHeight (72dp), so maxHeight (40dp) <= minHeight (72dp)
        mHelper.updatePlaceholderHeight(minHeight + 20);
        assertEquals(1.0f, content.getAlpha(), 0.01f);

        lock.unlock();
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

    @Test
    public void testDisableResizingMode_SizeUnchanged_ImmediatelyFadesInWithoutWaitingNextFrame() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        // WebContents and container have matching dimensions.
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        ResizeLock lock = mHelper.requestResize();
        clearInvocations(mMockThinWebView);

        // Size did not change; unlocking should trigger onNextFrameAfterResize directly
        // without registering a runOnNextFrame callback.
        lock.unlock();

        verify(mMockThinWebView, never()).runOnNextFrame(any());
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void testDisableResizingMode_ReenteredResizeModeBeforeFrameCallback() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        doAnswer(invocation -> null).when(mMockThinWebView).runOnNextFrame(any());

        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        ResizeLock lock1 = mHelper.requestResize();
        assertNotNull(mHelper.getPlaceholderCoordinator());
        View placeholder = mHelper.getPlaceholderCoordinator().getView();
        assertSame(placeholder, container.getChildAt(0));
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        // Container size changes while resizing mode is active.
        container.measure(
                View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 300, 400);

        ArgumentCaptor<Runnable> frameCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        lock1.unlock();

        verify(mMockThinWebView).runOnNextFrame(frameCallbackCaptor.capture());

        // Re-enter resizing mode before the captured next-frame callback executes.
        ResizeLock lock2 = mHelper.requestResize();
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        // Now run the stale frame callback from lock1.
        frameCallbackCaptor.getValue().run();

        // Placeholder should still remain visible because resizing mode is active again.
        assertEquals(View.VISIBLE, placeholder.getVisibility());

        lock2.unlock();
    }

    @Test
    public void testResizeLock_DoubleUnlockIsSafe() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 200);

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);

        // First unlock should transition out of resizing mode.
        lock.unlock();
        assertEquals(View.VISIBLE, mView.getVisibility());

        // Second unlock on the same lock should be idempotent and not crash.
        lock.unlock();
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    public void testRequestResize_NullThinWebView_ReturnsNull() {
        mHelper.reset();
        ResizeLock lock = mHelper.requestResize();
        assertNull(lock);
    }

    @Test
    public void testUpdateBounds_EpsilonLimitsTolerance() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        clearInvocations(mMockThinWebView);

        // 1. Dimensions within epsilon tolerance (<= 2px): should NOT resize
        container.measure(
                View.MeasureSpec.makeMeasureSpec(102, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(201, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 102, 201);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // 2. Width difference > 2px: SHOULD resize
        container.measure(
                View.MeasureSpec.makeMeasureSpec(103, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 103, 200);
        verify(mMockThinWebView).resizeWebContents(103, 200);

        // 3. Height difference > 2px: SHOULD resize
        clearInvocations(mMockThinWebView);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        container.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(203, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 100, 203);
        verify(mMockThinWebView).resizeWebContents(100, 203);
    }

    @Test
    public void testKeyboardShowing_FocusedSidePanel_Resizes() {
        createSidePanelHelperWithKeyboard(/* webContentsHeightPx= */ 400);
        mView.requestFocus();
        clearInvocations(mMockThinWebView);

        layoutResizingContainer(300, 200);

        verify(mMockThinWebView).resizeWebContents(300, 200);
    }

    @Test
    public void testKeyboardShowing_UnfocusedSidePanel_KeepsNoKeyboardHeight() {
        View otherView = createSidePanelHelperWithKeyboard(/* webContentsHeightPx= */ 200);
        otherView.requestFocus();
        clearInvocations(mMockThinWebView);

        layoutResizingContainer(300, 200);

        verify(mMockThinWebView).resizeWebContents(300, 400);
    }

    @Test
    public void testKeyboardShowing_FocusLoss_RestoresHeight() {
        View otherView = createSidePanelHelperWithKeyboard(/* webContentsHeightPx= */ 200);
        mView.requestFocus();
        layoutResizingContainer(300, 200);
        clearInvocations(mMockThinWebView);

        otherView.requestFocus();
        mHelper.onFocusChanged();

        verify(mMockThinWebView).resizeWebContents(300, 400);
    }

    @Test
    public void testKeyboardShowing_FocusGain_Shrinks() {
        View otherView = createSidePanelHelperWithKeyboard(/* webContentsHeightPx= */ 400);
        otherView.requestFocus();
        layoutResizingContainer(300, 200);
        clearInvocations(mMockThinWebView);

        mView.requestFocus();
        mHelper.onFocusChanged();

        verify(mMockThinWebView).resizeWebContents(300, 200);
    }

    @Test
    public void testKeyboardShowing_OverlayMode_DoesNotAddKeyboardHeight() {
        View otherView = createSidePanelHelperWithKeyboard(/* webContentsHeightPx= */ 400);
        otherView.requestFocus();
        when(mMockInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);
        clearInvocations(mMockThinWebView);

        layoutResizingContainer(300, 200);

        verify(mMockThinWebView).resizeWebContents(300, 200);
    }

    @Test
    public void testOnFocusChanged_ViewportSizeFixed_DoesNotResize() {
        createSidePanelHelperWithKeyboard(/* webContentsHeightPx= */ 200);
        mView.requestFocus();
        layoutResizingContainer(300, 200);
        assertNotNull(mHelper.requestResize());
        clearInvocations(mMockThinWebView);

        mView.clearFocus();
        mHelper.onFocusChanged();

        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    public void testOnFocusChanged_BottomSheet_DoesNotResize() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        clearInvocations(mMockThinWebView);

        mHelper.onFocusChanged();

        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    public void testResizingContainer_LayoutChange_RespectsViewportFixedLimit() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        ResizeLock lock = mHelper.requestResize();
        clearInvocations(mMockThinWebView);

        // While resizing mode is active (mIsViewportSizeFixed = true), layout changes
        // must not trigger updateBounds.
        container.measure(
                View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 300, 400);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // Disable resizing mode (mIsViewportSizeFixed = false).
        lock.unlock();
        verify(mMockThinWebView).resizeWebContents(300, 400);

        // Subsequent layout changes on the container should trigger updateBounds.
        clearInvocations(mMockThinWebView);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 300));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 400));

        container.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 500, 600);
        verify(mMockThinWebView).resizeWebContents(500, 600);
    }

    @Test
    public void testPlaceholderCoordinator_DelegatesToComponentProvider() {
        CoBrowseComponentProvider provider = mock(CoBrowseComponentProvider.class);
        ResizingPlaceholderCoordinator customCoordinator =
                mock(ResizingPlaceholderCoordinator.class);
        View customPlaceholderView = new View(mContext);
        when(customCoordinator.getView()).thenReturn(customPlaceholderView);
        when(provider.createResizingPlaceholderCoordinator(any(), anyInt(), anyInt()))
                .thenReturn(customCoordinator);

        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        false,
                        provider);
        verify(provider, never()).createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        assertNull(helper.getPlaceholderCoordinator());

        helper.setThinWebView(mMockThinWebView, mMockWebContents);
        assertNull(helper.getPlaceholderCoordinator());
        verify(provider, never()).createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());

        ResizeLock lock = helper.requestResize();
        assertNotNull(lock);
        verify(provider).createResizingPlaceholderCoordinator(mContext, Color.WHITE, Color.LTGRAY);
        assertSame(customCoordinator, helper.getPlaceholderCoordinator());
        verify(customCoordinator, never()).updateVisibleHeight(anyInt());

        helper.updatePlaceholderHeight(250);
        verify(customCoordinator).updateVisibleHeight(250);

        customPlaceholderView.setVisibility(View.VISIBLE);
        customPlaceholderView.setAlpha(0.4f);
        lock.unlock();
        helper.destroy();
        verify(customCoordinator).destroy();
        assertEquals(View.INVISIBLE, customPlaceholderView.getVisibility());
        assertEquals(1.0f, customPlaceholderView.getAlpha(), 0.01f);
        assertEquals(0, ((FrameLayout) helper.getResizingContainer()).getChildCount());
    }

    @Test
    public void testUpdatePlaceholderHeight_BeforeResize_CachesAndAppliesOnResize() {
        CoBrowseComponentProvider provider = mock(CoBrowseComponentProvider.class);
        ResizingPlaceholderCoordinator customCoordinator =
                mock(ResizingPlaceholderCoordinator.class);
        View customPlaceholderView = new View(mContext);
        when(customCoordinator.getView()).thenReturn(customPlaceholderView);
        when(provider.createResizingPlaceholderCoordinator(any(), anyInt(), anyInt()))
                .thenReturn(customCoordinator);

        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        false,
                        provider);

        helper.updatePlaceholderHeight(180);

        verify(provider, never()).createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        assertNull(helper.getPlaceholderCoordinator());
        verify(customCoordinator, never()).updateVisibleHeight(anyInt());

        helper.setThinWebView(mMockThinWebView, mMockWebContents);
        verify(provider, never()).createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        assertNull(helper.getPlaceholderCoordinator());

        ResizeLock lock = helper.requestResize();
        assertNotNull(lock);
        verify(provider).createResizingPlaceholderCoordinator(mContext, Color.WHITE, Color.LTGRAY);
        verify(customCoordinator).updateVisibleHeight(180);

        helper.updatePlaceholderHeight(320);
        verify(customCoordinator).updateVisibleHeight(320);
        helper.destroy();
    }

    @Test
    public void testPlaceholderCoordinator_NullCoordinator_WorksCleanly() {
        CoBrowseComponentProvider provider = mock(CoBrowseComponentProvider.class);
        when(provider.createResizingPlaceholderCoordinator(any(), anyInt(), anyInt()))
                .thenReturn(null);

        WebViewResizingHelper helper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        false,
                        provider);
        assertNull(helper.getPlaceholderCoordinator());
        FrameLayout container = (FrameLayout) helper.getResizingContainer();
        assertEquals(0, container.getChildCount());

        helper.setThinWebView(mMockThinWebView, mMockWebContents);
        assertEquals(1, container.getChildCount());

        helper.updatePlaceholderHeight(250);
        ResizeLock lock = helper.requestResize();
        assertNotNull(lock);
        lock.unlock();

        helper.reset();
        assertEquals(0, container.getChildCount());

        helper.destroy();
    }

    @Test
    public void testReset_CancelsPendingTimeoutRunnable() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        mView.layout(0, 0, 100, 200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        doAnswer(invocation -> null).when(mMockThinWebView).runOnNextFrame(any());

        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 100));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 200));

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 300, 400);

        lock.unlock();

        assertNotNull(mHelper.getPendingTimeoutRunnableForTesting());

        mHelper.reset();

        assertNull(mHelper.getPendingTimeoutRunnableForTesting());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetThinWebView_UpdatesSafeAreaInsetsWhenEdgeToEdgeEnabled() {
        when(mMockDisplayAndroid.getDipScale()).thenReturn(2.0f);

        int topMarginPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_web_ui_top_margin);
        int expectedTopDip = (int) Math.ceil(topMarginPx / 2.0f);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 0)));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetThinWebView_UpdatesBottomSafeAreaInsetAndObservesChanges() {
        when(mMockDisplayAndroid.getDipScale()).thenReturn(2.0f);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);

        int topMarginPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_web_ui_top_margin);
        int expectedTopDip = (int) Math.ceil(topMarginPx / 2.0f);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mMockWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Rect firstSafeArea = safeAreaCaptor.getValue();
        assertEquals(new Rect(0, expectedTopDip, 0, 24), firstSafeArea);

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        ArgumentCaptor<BottomSheetObserver> bottomSheetObserverCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());
        verify(mMockBottomSheetController).addObserver(bottomSheetObserverCaptor.capture());

        clearInvocations(mMockWebContents);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(0);
        insetObserverCaptor.getValue().onKeyboardInsetChanged(300);
        verify(mMockWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        assertSame(firstSafeArea, safeAreaCaptor.getValue());
        assertEquals(new Rect(0, expectedTopDip, 0, 0), safeAreaCaptor.getValue());

        clearInvocations(mMockWebContents);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(60);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 30)));

        clearInvocations(mMockWebContents);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(0);
        bottomSheetObserverCaptor.getValue().onContainerBottomMarginChanged(100);
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 0)));

        clearInvocations(mMockWebContents);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(40);
        bottomSheetObserverCaptor.getValue().onSheetContentChanged(/* newContent= */ null);
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 20)));

        clearInvocations(mMockWebContents);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        mAnimationListenerCaptor.getValue().onPrepare(/* animation= */ null);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(80);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());

        mAnimationListenerCaptor.getValue().onEnd(/* animation= */ null);
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 40)));

        clearInvocations(mMockWebContents);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 40)));

        WebContents secondWebContents = mock(WebContents.class);
        mHelper.setThinWebView(mMockThinWebView, secondWebContents);
        verify(secondWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 40)));

        clearInvocations(mMockWebContents);
        mHelper.reset();
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 40)));
        clearInvocations(mMockWebContents);

        ArgumentCaptor<ActivityStateObserver> activityObserverCaptor =
                ArgumentCaptor.forClass(ActivityStateObserver.class);
        verify(mMockWindowAndroid).addActivityStateObserver(activityObserverCaptor.capture());
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(120);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(80);
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.RESUMED);
        activityObserverCaptor.getValue().onActivityResumed();
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, expectedTopDip, 0, 40)));

        clearInvocations(mMockWebContents);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(100);
        when(mMockWebContents.isDestroyed()).thenReturn(true);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());

        mHelper.destroy();
        verify(mMockInsetObserver).removeObserver(insetObserverCaptor.getValue());
        verify(mMockBottomSheetController).removeObserver(bottomSheetObserverCaptor.getValue());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET, ChromeFeatureList.GLIC_NO_WEBVIEW})
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE)
    public void testSetThinWebView_DoesNotUpdateSafeAreaInsetsWhenEdgeToEdgeDisabled() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());
        verify(mMockInsetObserver, never()).addObserver(any());
        verify(mMockBottomSheetController, never()).addObserver(any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE
    })
    @DisableFeatures(ChromeFeatureList.GLIC_NO_WEBVIEW)
    public void testSetThinWebView_DoesNotUpdateSafeAreaInsetsWhenGlicNoWebviewDisabled() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());
        verify(mMockInsetObserver, never()).addObserver(any());
        verify(mMockBottomSheetController, never()).addObserver(any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetThinWebView_SidePanel_DoesNotUpdateSafeAreaInsets() {
        clearInvocations(mMockInsetObserver, mMockBottomSheetController);
        WebViewResizingHelper sidePanelHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ true,
                        mMockComponentProvider);

        sidePanelHelper.setThinWebView(mMockThinWebView, mMockWebContents);

        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());
        verify(mMockInsetObserver, never()).addObserver(any());
        verify(mMockBottomSheetController, never()).addObserver(any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testPlaceholderCoordinator_UpdatesBottomInset() {
        ResizingPlaceholderCoordinator coordinator = mock(ResizingPlaceholderCoordinator.class);
        doReturn(coordinator)
                .when(mMockComponentProvider)
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        verify(coordinator).updateBottomInset(48);

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());

        ArgumentCaptor<BottomSheetObserver> sheetObserverCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(sheetObserverCaptor.capture());

        // Unchanged bottom inset does not invoke updateBottomInset.
        clearInvocations(coordinator);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(coordinator, never()).updateBottomInset(anyInt());

        // Changed bottom inset invokes updateBottomInset.
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(72);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(coordinator).updateBottomInset(72);

        // Non-zero bottom margin with inset 0 updates bottom inset.
        clearInvocations(coordinator);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(0);
        sheetObserverCaptor.getValue().onContainerBottomMarginChanged(100);
        verify(coordinator).updateBottomInset(0);

        lock.unlock();
        mHelper.destroy();

        // Side panel helper never propagates bottom inset.
        clearInvocations(mMockBottomSheetController);
        ResizingPlaceholderCoordinator sidePanelCoordinator =
                mock(ResizingPlaceholderCoordinator.class);
        doReturn(sidePanelCoordinator)
                .when(mMockComponentProvider)
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        WebViewResizingHelper sidePanelHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ true,
                        mMockComponentProvider);
        sidePanelHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock sideLock = sidePanelHelper.requestResize();
        assertNotNull(sideLock);
        verify(sidePanelCoordinator, never()).updateBottomInset(anyInt());
        sideLock.unlock();
        sidePanelHelper.destroy();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testBottomSheetObserver_SheetContentOrMarginChanged_UpdatesBounds() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());
        BottomSheetObserver observer = observerCaptor.getValue();

        clearInvocations(mMockThinWebView);
        observer.onSheetContentChanged(mock(BottomSheetContent.class));
        verify(mMockThinWebView).resizeWebContents(1080, 2000);

        clearInvocations(mMockThinWebView);
        observer.onContainerBottomMarginChanged(40);
        verify(mMockThinWebView).resizeWebContents(1080, 2000);
    }

    @Test
    public void testImeAnimation_KeyboardOpens_ResizesOnEnd() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 1080));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 2000));

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        WindowInsetsAnimationListener listener = mAnimationListenerCaptor.getValue();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        clearInvocations(mMockThinWebView);
        listener.onPrepare(animation);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onStart(animation, bounds);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onProgress(mock(WindowInsetsCompat.class), List.of(animation));
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onEnd(animation);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
    }

    @Test
    public void testImeAnimation_KeyboardCloses_ResizesOnEnd() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 1080));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        WindowInsetsAnimationListener listener = mAnimationListenerCaptor.getValue();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        clearInvocations(mMockThinWebView);
        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onEnd(animation);
        verify(mMockThinWebView).resizeWebContents(1080, 2000);
    }

    @Test
    public void testImeAnimation_MidSessionHeightChange_ResizesOnEnd() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 1080));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        WindowInsetsAnimationListener listener = mAnimationListenerCaptor.getValue();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 800), Insets.of(0, 0, 0, 950));

        clearInvocations(mMockThinWebView);
        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1050, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1050);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onEnd(animation);
        verify(mMockThinWebView).resizeWebContents(1080, 1050);
    }

    @Test
    public void testImeAnimation_OverlappingAnimations_ResizesOnEnd() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        WindowInsetsAnimationListener listener = mAnimationListenerCaptor.getValue();

        var anim1 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var anim2 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(anim1);
        listener.onStart(anim1, bounds);
        listener.onPrepare(anim2);
        listener.onStart(anim2, bounds);

        clearInvocations(mMockThinWebView);
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onEnd(anim1);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
    }

    /**
     * Replaces {@link #mHelper} with a side panel helper attached to the activity, with a 200px
     * keyboard showing and the WebContents at {@code webContentsHeightPx}.
     *
     * @return A focusable view outside the side panel.
     */
    private View createSidePanelHelperWithKeyboard(int webContentsHeightPx) {
        mHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ true,
                        mMockComponentProvider);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        View otherView = new View(mContext);
        otherView.setFocusable(true);
        otherView.setFocusableInTouchMode(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            FrameLayout root = new FrameLayout(activity);
                            root.addView(
                                    mHelper.getResizingContainer(),
                                    new FrameLayout.LayoutParams(300, 200));
                            root.addView(otherView);
                            activity.setContentView(root);
                        });

        mKeyboardInsetSupplier.set(200);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 300));
        when(mMockWebContents.getHeight())
                .thenReturn(ViewUtils.pxToDp(mContext, webContentsHeightPx));
        return otherView;
    }

    private void layoutResizingContainer(int width, int height) {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, width, height);
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetBottomSafeAreaInsetObserver_NotifiedOnSet() {
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        Callback<Integer> observer = mock(Callback.class);

        mHelper.setBottomSafeAreaInsetObserver(observer);
        verify(observer).onResult(48);
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetBottomSafeAreaInsetObserver_NotifiedOnInsetChange() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());

        ArgumentCaptor<BottomSheetObserver> sheetObserverCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(sheetObserverCaptor.capture());

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        Callback<Integer> observer = mock(Callback.class);
        mHelper.setBottomSafeAreaInsetObserver(observer);
        verify(observer).onResult(48);

        // Same value -> no extra call.
        clearInvocations(observer);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(observer, never()).onResult(anyInt());

        // New value -> call via inset-change path.
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(72);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(observer).onResult(72);

        // Same value on margin change -> no extra call.
        clearInvocations(observer);
        sheetObserverCaptor.getValue().onContainerBottomMarginChanged(100);
        verify(observer, never()).onResult(anyInt());

        // New value -> call via onContainerBottomMarginChanged.
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(0);
        sheetObserverCaptor.getValue().onContainerBottomMarginChanged(100);
        verify(observer).onResult(0);

        lock.unlock();
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetBottomSafeAreaInsetObserver_NullStopsCalls() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        Callback<Integer> observer = mock(Callback.class);
        mHelper.setBottomSafeAreaInsetObserver(observer);
        verify(observer).onResult(48);

        mHelper.setBottomSafeAreaInsetObserver(null);
        clearInvocations(observer);

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(72);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(observer, never()).onResult(anyInt());

        lock.unlock();
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetBottomSafeAreaInsetObserver_DestroyClearsObserver() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        Callback<Integer> observer = mock(Callback.class);
        mHelper.setBottomSafeAreaInsetObserver(observer);
        verify(observer).onResult(48);

        clearInvocations(observer);
        mHelper.destroy();

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(72);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(observer, never()).onResult(anyInt());
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetBottomSafeAreaInsetObserver_DoesNotCallSetDisplayCutoutSafeArea() {
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        clearInvocations(mMockWebContents);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        Callback<Integer> observer = mock(Callback.class);

        mHelper.setBottomSafeAreaInsetObserver(observer);

        verify(observer).onResult(48);
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void
            testSetBottomSafeAreaInsetObserver_AfterDestroy_SettingNullDoesNotQueryBottomInset() {
        mHelper.destroy();
        clearInvocations(mMockBottomSheetController);

        mHelper.setBottomSafeAreaInsetObserver(null);
        verify(mMockBottomSheetController, never()).getEdgeToEdgeBottomInset();
    }

    @Test
    @SuppressWarnings("unchecked")
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testSetBottomSafeAreaInsetObserver_SidePanel_NeverCalled() {
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        WebViewResizingHelper sidePanelHelper =
                new WebViewResizingHelper(
                        mContainerView,
                        mMockWindowAndroid,
                        mMockBottomSheetController,
                        Color.WHITE,
                        Color.LTGRAY,
                        /* isSidePanel= */ true,
                        mMockComponentProvider);
        Callback<Integer> observer = mock(Callback.class);
        sidePanelHelper.setBottomSafeAreaInsetObserver(observer);
        verify(observer, never()).onResult(anyInt());
        sidePanelHelper.destroy();
    }

    @Test
    @SuppressWarnings("unchecked")
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE)
    public void testSetBottomSafeAreaInsetObserver_NonEdgeToEdge_NeverCalled() {
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        Callback<Integer> observer = mock(Callback.class);
        mHelper.setBottomSafeAreaInsetObserver(observer);
        verify(observer, never()).onResult(anyInt());
        mHelper.destroy();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testUpdateBounds_WithHeightAndSafeAreaOverrides() {
        ResizingPlaceholderCoordinator coordinator = mock(ResizingPlaceholderCoordinator.class);
        doReturn(coordinator)
                .when(mMockComponentProvider)
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        layoutResizingContainer(300, 400);

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        verify(coordinator).updateBottomInset(48);
        clearInvocations(coordinator, mMockThinWebView);

        boolean sizeChanged =
                mHelper.updateBounds(
                        /* ignoreCache= */ true,
                        /* heightOverridePx= */ 500,
                        /* bottomSafeAreaOverridePx= */ 0);

        assertTrue(sizeChanged);
        verify(mMockThinWebView).resizeWebContents(300, 500);
        verify(coordinator).updateBottomInset(0);

        clearInvocations(coordinator, mMockThinWebView);
        boolean defaultBoundsChanged =
                mHelper.updateBounds(
                        /* ignoreCache= */ true,
                        WebViewResizingHelper.NO_HEIGHT_OVERRIDE,
                        WebViewResizingHelper.NO_SAFE_AREA_OVERRIDE);
        assertTrue(defaultBoundsChanged);
        verify(mMockThinWebView).resizeWebContents(300, 400);
        verify(coordinator).updateBottomInset(48);

        clearInvocations(coordinator);
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);
        mHelper.updateSafeAreaInsets(/* ignoreCache= */ true, /* bottomSafeAreaOverridePx= */ 32);
        verify(coordinator, never()).updateBottomInset(anyInt());

        lock.unlock();
    }

    private WindowInsetsAnimationListener setupSmoothImeEnv(
            @SheetState int sheetState,
            int maxSheetHeight,
            int top,
            int systemBarBottom,
            int imeBottom) {
        mCurrentSheetState = sheetState;
        mTargetImeInsetPx = imeBottom;
        when(mMockBottomSheetController.getSheetState()).thenAnswer(inv -> mCurrentSheetState);
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(maxSheetHeight);
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mMockSheetContent);
        lenient().doReturn(mContainerView).when(mMockSheetContent).getContentView();
        when(mMockSheetContent.getFullHeightRatio())
                .thenReturn((float) BottomSheetContent.HeightMode.RESIZE_CONTENT);
        when(mMockSheetContent.getHalfHeightRatio()).thenReturn(0.75f);
        when(mMockSheetContent.coversBottomControls())
                .thenReturn(TabBottomSheetUtils.isEdgeToEdgeEnabled());
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset())
                .thenReturn(imeBottom > 0 ? 0 : systemBarBottom);
        when(mMockBottomSheetController.hasBottomInset()).thenReturn(systemBarBottom > 0);

        when(mMockBottomSheetController.getProjectedContentHeightForIme())
                .thenAnswer(
                        inv -> {
                            if (mCurrentSheetState == SheetState.HIDDEN
                                    || mCurrentSheetState == SheetState.PEEK) {
                                return 0;
                            }
                            int targetIme = mTargetImeInsetPx;
                            int effectiveMax = maxSheetHeight;
                            int effectiveIme = targetIme;
                            if (top > 0 && targetIme > 0) {
                                effectiveIme = targetIme + top;
                            }
                            int projectedContainer = Math.max(0, effectiveMax - effectiveIme);
                            if (mCurrentSheetState == SheetState.HALF) {
                                return (int) Math.floor(projectedContainer * 0.75f);
                            }
                            return projectedContainer;
                        });

        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.systemBars(),
                                Insets.of(0, top, 0, systemBarBottom))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, imeBottom))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insets);

        mMockDecorView.layout(0, 0, 1080, 2000);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 1080));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 2000));

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);
        doAnswer(
                        invocation -> {
                            int w = invocation.getArgument(0);
                            int h = invocation.getArgument(1);
                            when(mMockWebContents.getWidth())
                                    .thenReturn(ViewUtils.pxToDp(mContext, w));
                            when(mMockWebContents.getHeight())
                                    .thenReturn(ViewUtils.pxToDp(mContext, h));
                            return null;
                        })
                .when(mMockThinWebView)
                .resizeWebContents(anyInt(), anyInt());
        clearInvocations(mMockThinWebView, mMockWebContents);

        verify(mMockInsetObserver)
                .addWindowInsetsAnimationListener(mAnimationListenerCaptor.capture());
        WindowInsetsAnimationListener realListener = mAnimationListenerCaptor.getValue();

        return new WindowInsetsAnimationListener() {
            private boolean mKeyboardOpen = imeBottom > 0;

            @Override
            public void onPrepare(WindowInsetsAnimationCompat animation) {
                realListener.onPrepare(animation);
            }

            @Override
            public void onStart(
                    WindowInsetsAnimationCompat animation,
                    WindowInsetsAnimationCompat.BoundsCompat bounds) {
                if (bounds == null || TabBottomSheetUtils.isActivityInactive(mMockWindowAndroid)) {
                    realListener.onStart(animation, bounds);
                    return;
                }
                int upper = bounds.getUpperBound().bottom;
                int lower = bounds.getLowerBound().bottom;
                if (upper > 0 && lower > 0) {
                    mKeyboardOpen = true;
                    mTargetImeInsetPx = upper;
                } else if (!mKeyboardOpen && upper > 0) {
                    mKeyboardOpen = true;
                    mTargetImeInsetPx = upper;
                    WindowInsetsCompat insetsWithTarget =
                            new WindowInsetsCompat.Builder()
                                    .setInsets(
                                            WindowInsetsCompat.Type.systemBars(),
                                            Insets.of(0, top, 0, systemBarBottom))
                                    .setInsets(
                                            WindowInsetsCompat.Type.ime(),
                                            Insets.of(0, 0, 0, upper))
                                    .build();
                    when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsWithTarget);
                } else if (mKeyboardOpen) {
                    mKeyboardOpen = false;
                    mTargetImeInsetPx = 0;
                }
                realListener.onStart(animation, bounds);
            }

            @Override
            public void onProgress(
                    WindowInsetsCompat insets,
                    List<WindowInsetsAnimationCompat> runningAnimations) {
                realListener.onProgress(insets, runningAnimations);
            }

            @Override
            public void onEnd(WindowInsetsAnimationCompat animation) {
                realListener.onEnd(animation);
            }
        };
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_KeyboardOpens_ResizesWebContentsInOneStep_NonEdgeToEdge() {
        WindowInsetsAnimationListener listener =
                setupSmoothImeEnv(SheetState.FULL, 2000, 100, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 1082);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1082, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1082);

        listener.onProgress(mock(WindowInsetsCompat.class), List.of(animation));
        listener.onEnd(animation);
        verify(mMockThinWebView, times(1)).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_KeyboardOpens_ResizesWebContentsInOneStep_EdgeToEdge() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
        verify(mMockWebContents).setDisplayCutoutSafeArea(eq(new Rect(0, 18, 0, 0)));

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);

        listener.onEnd(animation);
        verify(mMockThinWebView, times(1)).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_KeyboardCloses_ExpandsToUnshrunkBoundsInOneStep() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(80);

        var animationOpen =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsOpen =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animationOpen);

        WindowInsetsCompat insetsOpen =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);

        listener.onStart(animationOpen, boundsOpen);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
        verify(mMockWebContents).setDisplayCutoutSafeArea(new Rect(0, 18, 0, 0));

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 1080));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(1200);
        listener.onEnd(animationOpen);

        clearInvocations(mMockThinWebView, mMockWebContents);
        var animationClose =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsClose =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animationClose);

        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);

        listener.onStart(animationClose, boundsClose);
        verify(mMockThinWebView).resizeWebContents(1080, 2000);
        verify(mMockWebContents).setDisplayCutoutSafeArea(new Rect(0, 18, 0, 80));

        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);
        listener.onEnd(animationClose);
        verify(mMockThinWebView, times(1)).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_FloatingKeyboard_ZeroHeight_DoesNotResizeEarly() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 0));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        clearInvocations(mMockThinWebView);
        ShadowLooper.idleMainLooper(650, TimeUnit.MILLISECONDS);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_SmallScreen_PromotesToFullHeight() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.HALF, 2000, 0, 80, 0);
        when(mMockBottomSheetController.isSmallScreen()).thenReturn(true);
        doReturn(1200).when(mMockBottomSheetController).getProjectedContentHeightForIme();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_ContentOptions_CoversBottomControlsAndMaxResizeRatio() {
        WindowInsetsAnimationListener listener =
                setupSmoothImeEnv(SheetState.FULL, 2000, 50, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        doReturn(960).when(mMockBottomSheetController).getProjectedContentHeightForIme();
        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 960);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_PendingLayout_DefersUpdateBounds() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
        clearInvocations(mMockThinWebView);

        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 2000));

        FrameLayout parent = attachResizingContainerToParent(1080, 1200);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        ViewUtils.requestLayout(container, "WebViewResizingHelperUnitTest");

        listener.onEnd(animation);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        layoutViewAndAncestors(container, 1080, 1200);
        container.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_PendingAncestorLayout_DefersUpdateBounds() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
        clearInvocations(mMockThinWebView);

        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 2000));

        FrameLayout parent = new FrameLayout(mContext);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        if (container.getParent() != null) {
            ((ViewGroup) container.getParent()).removeView(container);
        }
        parent.addView(container);
        mActivity.setContentView(parent);
        ShadowLooper.runUiThreadTasks();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        parent.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        parent.layout(0, 0, 1080, 1200);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        listener.onEnd(animation);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        layoutViewAndAncestors(container, 1080, 1200);
        container.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_WatchdogExpires_ResetsPausedAndUpdateBounds() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        clearInvocations(mMockThinWebView);

        ShadowLooper.idleMainLooper(650, TimeUnit.MILLISECONDS);
        verify(mMockThinWebView).resizeWebContents(1080, 2000);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_InactiveActivityOrLocked_SuppressesPrediction() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.HALF, 2000, 0, 80, 0);

        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);
        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        clearInvocations(mMockThinWebView);
        ShadowLooper.idleMainLooper(650, TimeUnit.MILLISECONDS);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
        listener.onEnd(animation);

        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.RESUMED);
        var animOpen = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsOpen =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        WindowInsetsCompat insetsOpen =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);

        listener.onPrepare(animOpen);
        listener.onStart(animOpen, boundsOpen);
        listener.onEnd(animOpen);

        clearInvocations(mMockThinWebView);
        ResizeLock lock = mHelper.requestResize();
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.SCROLLING);

        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        var animClose = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsClose =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animClose);
        listener.onStart(animClose, boundsClose);
        listener.onEnd(animClose);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        clearInvocations(mMockThinWebView);
        mCurrentSheetState = SheetState.FULL;
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        layoutViewAndAncestors(mHelper.getResizingContainer(), 1080, 2000);
        lock.unlock();
        verify(mMockThinWebView).resizeWebContents(1080, 2000);

        clearInvocations(mMockThinWebView);
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        var animOpen2 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        listener.onPrepare(animOpen2);
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);
        listener.onStart(animOpen2, boundsOpen);
        verify(mMockThinWebView).resizeWebContents(1080, 1182);
        listener.onEnd(animOpen2);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW
    })
    public void testImeAnimation_ColdStartWithImeOpen_ExpandsOnDismiss() {
        WindowInsetsAnimationListener listener =
                setupSmoothImeEnv(SheetState.FULL, 1200, 0, 80, 800);
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);

        clearInvocations(mMockThinWebView, mMockWebContents);
        var animClose = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsClose =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animClose);

        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        doReturn(2000).when(mMockBottomSheetController).getProjectedContentHeightForIme();
        doReturn(80).when(mMockBottomSheetController).getEdgeToEdgeBottomInset();

        listener.onStart(animClose, boundsClose);

        verify(mMockThinWebView).resizeWebContents(1080, 2000);
        verify(mMockWebContents).setDisplayCutoutSafeArea(new Rect(0, 18, 0, 80));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE)
    public void testSheetObserver_StateChangedToPeekOrHidden_ClearsSheetStateBeforeIme() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.HALF, 2000, 0, 80, 0);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());
        BottomSheetObserver observer = observerCaptor.getValue();

        var animOpen = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsOpen =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        listener.onPrepare(animOpen);

        WindowInsetsCompat insetsOpen =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);

        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.SWIPE);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        doReturn(1200).when(mMockBottomSheetController).getProjectedContentHeightForIme();
        clearInvocations(mMockThinWebView);
        listener.onStart(animOpen, boundsOpen);
        verify(mMockThinWebView).resizeWebContents(1080, 1182);
        listener.onEnd(animOpen);

        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 2000));
        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 80))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HALF);

        var animOpen2 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        listener.onPrepare(animOpen2);
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);

        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        mHelper.getResizingContainer().layout(0, 0, 1080, 2000);
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        doReturn(1200).when(mMockBottomSheetController).getProjectedContentHeightForIme();
        clearInvocations(mMockThinWebView);
        listener.onStart(animOpen2, boundsOpen);
        verify(mMockThinWebView).resizeWebContents(1080, 1182);
        listener.onEnd(animOpen2);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_KeyboardCloses_FromHalfState_RestoresHalfHeight() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.HALF, 2000, 0, 0, 0);
        when(mMockBottomSheetController.isSmallScreen()).thenReturn(true);
        doReturn(1200).when(mMockBottomSheetController).getProjectedContentHeightForIme();

        var animationOpen =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsOpen =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animationOpen);
        WindowInsetsCompat insetsOpen =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);
        listener.onStart(animationOpen, boundsOpen);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 1080));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(1200);
        listener.onEnd(animationOpen);

        clearInvocations(mMockThinWebView);
        var animationClose =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsClose =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animationClose);
        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        when(mMockBottomSheetController.isSmallScreen()).thenReturn(true);
        doReturn(1500).when(mMockBottomSheetController).getProjectedContentHeightForIme();
        listener.onStart(animationClose, boundsClose);
        verify(mMockThinWebView).resizeWebContents(1080, 1500);

        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1500, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1500);
        listener.onEnd(animationClose);
        verify(mMockThinWebView, times(1)).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_MidSessionHeightChangeAndCompactIme() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 0, 0);

        var animOpen = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsOpen =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        listener.onPrepare(animOpen);
        listener.onStart(animOpen, boundsOpen);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);

        clearInvocations(mMockThinWebView);
        WindowInsetsCompat insets950 =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 950))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insets950);
        var animGrow = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsGrow =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 800), Insets.of(0, 0, 0, 950));
        listener.onPrepare(animGrow);
        listener.onStart(animGrow, boundsGrow);
        verify(mMockThinWebView).resizeWebContents(1080, 1050);

        clearInvocations(mMockThinWebView);
        WindowInsetsCompat insetsCurrent800 =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsCurrent800);
        var animShrink = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsShrink =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 950), Insets.of(0, 0, 0, 800));
        listener.onPrepare(animShrink);
        listener.onStart(animShrink, boundsShrink);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);

        clearInvocations(mMockThinWebView);
        var animConsecutive =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsConsecutive =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 800), Insets.of(0, 0, 0, 800));
        listener.onPrepare(animConsecutive);
        listener.onStart(animConsecutive, boundsConsecutive);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        mHelper.reset();
        listener = setupSmoothImeEnv(SheetState.HALF, 2000, 0, 0, 0);
        WindowInsetsCompat insets100 =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 100))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insets100);
        var animCompact = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsCompact =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 100));
        listener.onPrepare(animCompact);
        listener.onStart(animCompact, boundsCompact);
        verify(mMockThinWebView).resizeWebContents(1080, 1425);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_ResetAndDestroy_CleansUpState() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.HALF, 2000, 0, 0, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        listener.onPrepare(animation);

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        ViewUtils.requestLayout(container, "WebViewResizingHelperUnitTest");

        listener.onEnd(animation);
        mHelper.reset();

        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1100, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1100);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        clearInvocations(mMockThinWebView);
        mCurrentSheetState = SheetState.FULL;
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        var animation2 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds2 =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        listener.onPrepare(animation2);
        listener.onStart(animation2, bounds2);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);

        mHelper.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW)
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE)
    public void testImeAnimation_DisabledFeatureFlag_PreservesLegacyBehavior() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        clearInvocations(mMockThinWebView);
        ShadowLooper.idleMainLooper(650, TimeUnit.MILLISECONDS);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1200, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 1200);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        listener.onEnd(animation);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
        mHelper.reset();
        mHelper.destroy();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_CloseWithStaleInsets_RestoresPostCloseHeightAndSafeArea() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 48, 0);

        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        WindowInsetsCompat insets800 =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 48))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insets800);

        var animOpen = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsOpen =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        listener.onPrepare(animOpen);
        listener.onStart(animOpen, boundsOpen);
        listener.onEnd(animOpen);

        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(1200);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        clearInvocations(mMockThinWebView, mMockWebContents);

        var animClose = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var boundsClose =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        doReturn(2000).when(mMockBottomSheetController).getProjectedContentHeightForIme();
        listener.onPrepare(animClose);
        WindowInsetsCompat insetsClose0 =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 48))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose0);
        listener.onStart(animClose, boundsClose);

        verify(mMockThinWebView).resizeWebContents(1080, 2000);
        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mMockWebContents, atLeastOnce()).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        assertEquals(48, safeAreaCaptor.getValue().bottom);

        // onEnd() runs while raw insets are still stale (> 0).
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insets800);
        clearInvocations(mMockThinWebView, mMockWebContents);
        listener.onEnd(animClose);

        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testDeferredLayout_CancelsWatchdog() {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        if (container.getParent() != null) {
            ((ViewGroup) container.getParent()).removeView(container);
        }
        FrameLayout parent = new FrameLayout(mContext);
        parent.addView(container);
        mActivity.setContentView(parent);
        ShadowLooper.runUiThreadTasks();
        parent.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        parent.layout(0, 0, 1080, 2000);
        container.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, 1080, 2000);

        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 0, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        listener.onEnd(animation);

        clearInvocations(mMockThinWebView);

        layoutViewAndAncestors(container, 1080, 2000);
        container.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(mMockThinWebView).resizeWebContents(1080, 2000);

        clearInvocations(mMockThinWebView);

        ShadowLooper.idleMainLooper(650, TimeUnit.MILLISECONDS);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    private void layoutViewAndAncestors(View view, int width, int height) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, width, height);
        ViewParent parent = view.getParent();
        while (parent instanceof View) {
            ((View) parent)
                    .measure(
                            View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                            View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
            ((View) parent).layout(0, 0, width, height);
            parent = parent.getParent();
        }
    }

    private FrameLayout attachResizingContainerToParent(int width, int height) {
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();
        if (container.getParent() != null) {
            ((ViewGroup) container.getParent()).removeView(container);
        }
        FrameLayout parent = new FrameLayout(mContext);
        parent.addView(container);
        mActivity.setContentView(parent);
        ShadowLooper.runUiThreadTasks();
        layoutViewAndAncestors(container, width, height);
        return parent;
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testDetachedTeardown_RemovesListenerFromCapturedObserver() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 0, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        listener.onEnd(animation);

        parent.removeView(container);
        mHelper.destroy();

        clearInvocations(mMockThinWebView);
        parent.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testIntermediateCallbacks_DoNotTriggerUpdateBoundsWhenLayoutPending() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);

        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 0, 0);

        ArgumentCaptor<BottomSheetObserver> sheetObserverCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(sheetObserverCaptor.capture());
        BottomSheetObserver sheetObserver = sheetObserverCaptor.getValue();

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());
        WindowInsetObserver windowInsetObserver = insetObserverCaptor.getValue();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        listener.onEnd(animation);

        clearInvocations(mMockThinWebView);

        sheetObserver.onContainerBottomMarginChanged(100);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        windowInsetObserver.onKeyboardInsetChanged(800);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testSheetHiddenOrContentChanged_ClearsWatchdogAndLayoutListener() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);

        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 0, 0);

        ArgumentCaptor<BottomSheetObserver> sheetObserverCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(sheetObserverCaptor.capture());
        BottomSheetObserver sheetObserver = sheetObserverCaptor.getValue();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        listener.onEnd(animation);

        clearInvocations(mMockThinWebView);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HIDDEN);
        sheetObserver.onSheetStateChanged(SheetState.HIDDEN, 0);
        var otherContent = mock(BottomSheetContent.class);
        when(otherContent.getContentView()).thenReturn(new View(mContext));
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(otherContent);
        sheetObserver.onSheetContentChanged(otherContent);

        parent.layout(0, 0, 1080, 2000);
        ShadowLooper.idleMainLooper(650, TimeUnit.MILLISECONDS);
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testImeAnimation_BottomControlsHideOnOpen_ResizesToUnshrunkHeightInOneStep() {
        when(mMockBottomSheetController.getContainerBottomMargin()).thenReturn(100);
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 1900, 0, 0, 0);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());
        BottomSheetObserver observer = observerCaptor.getValue();

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);

        when(mMockBottomSheetController.getContainerBottomMargin()).thenReturn(0);
        observer.onContainerBottomMarginChanged(0);
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(1900);
        doReturn(1200).when(mMockBottomSheetController).getProjectedContentHeightForIme();

        listener.onStart(animation, bounds);
        verify(mMockThinWebView).resizeWebContents(1080, 1200);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testDeferredOnEnd_PreservesUnshrunkEdgeToEdgeBottomSafeArea() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        WindowInsetsAnimationListener listener =
                setupSmoothImeEnv(SheetState.FULL, 2000, 0, 48, 800);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        listener.onPrepare(animation);
        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        doReturn(48).when(mMockBottomSheetController).getEdgeToEdgeBottomInset();
        clearInvocations(mMockWebContents);
        listener.onStart(animation, bounds);

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mMockWebContents, atLeastOnce()).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        assertEquals(48, safeAreaCaptor.getValue().bottom);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        clearInvocations(mMockWebContents);

        listener.onEnd(animation);
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());

        // Also trigger the pending onGlobalLayout() callback while raw insets are stale and
        // verify safe-area is not shrunk back to 0px.
        when(mMockInsetObserver.getLastRawWindowInsets())
                .thenReturn(
                        new WindowInsetsCompat.Builder()
                                .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                                .build());
        clearInvocations(mMockWebContents);
        layoutViewAndAncestors(container, 1080, 2000);
        container.getViewTreeObserver().dispatchOnGlobalLayout();
        verify(mMockWebContents, never()).setDisplayCutoutSafeArea(any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testDisableResizingMode_WhileImePaused_RecomputesProjectedHeightWithTopMargin() {
        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        // WebContents starts at the post-keyboard height (1200px) prior to unlock.
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        ResizeLock lock = mHelper.requestResize();
        layoutViewAndAncestors(mHelper.getResizingContainer(), 1080, 1200);

        clearInvocations(mMockThinWebView);

        // While locked and paused, sheet content changes projection from 1200 to 1000.
        when(mMockBottomSheetController.getProjectedContentHeightForIme()).thenReturn(1000);

        lock.unlock();

        // 1000 - 18px top margin = 982px.
        verify(mMockThinWebView).resizeWebContents(1080, 982);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testDisableResizingMode_AfterImeEndUnderLock_DefersWhenLayoutPending() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        // WebContents starts at the post-keyboard height (1200px) prior to unlock.
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        ResizeLock lock = mHelper.requestResize();
        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        listener.onEnd(animation);

        clearInvocations(mMockThinWebView);

        // Parent layout is pending when unlocking.
        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        lock.unlock();

        // Unlocking should not resize with unlaid-out dimensions.
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // Completing layout triggers reconciliation.
        layoutViewAndAncestors(container, 1080, 2000);
        container.getViewTreeObserver().dispatchOnGlobalLayout();

        verify(mMockThinWebView).resizeWebContents(1080, 2000);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void
            testDisableResizingMode_WhenViewTreeObserverNotAlive_ReconcilesImmediatelyAndFadesIn() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 1200));

        ResizeLock lock = mHelper.requestResize();
        clearInvocations(mMockThinWebView);

        // Parent layout is pending, but container is detached so its ViewTreeObserver is dead.
        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        parent.removeView(container);

        lock.unlock();

        // Since ViewTreeObserver is not alive, it falls back to immediate reconciliation.
        verify(mMockThinWebView).runOnNextFrame(any());
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testOnKeyboardInsetChanged_DuringDeferredImeClose_UpdatesPendingSafeInset() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);
        FrameLayout container = (FrameLayout) mHelper.getResizingContainer();

        WindowInsetsAnimationListener listener = setupSmoothImeEnv(SheetState.FULL, 2000, 0, 48, 0);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds =
                new WindowInsetsAnimationCompat.BoundsCompat(
                        Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        // Start open animation and complete it.
        listener.onPrepare(animation);
        listener.onStart(animation, bounds);
        listener.onEnd(animation);

        // Now start close animation with pending layout on container.
        var closeAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        listener.onPrepare(closeAnim);
        WindowInsetsCompat insetsClose =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 0, 0, 48))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsClose);
        doReturn(48).when(mMockBottomSheetController).getEdgeToEdgeBottomInset();
        listener.onStart(closeAnim, bounds);

        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        listener.onEnd(closeAnim);

        // Window insets observer receives keyboard inset change before layout settles.
        mHelper.getWindowInsetObserverForTesting().onKeyboardInsetChanged(0);

        // Complete layout.
        layoutViewAndAncestors(container, 1080, 2000);
        container.getViewTreeObserver().dispatchOnGlobalLayout();

        // Verifies safe area insets were updated to post-close value.
        ArgumentCaptor<Rect> captor = ArgumentCaptor.forClass(Rect.class);
        verify(mMockWebContents, atLeastOnce()).setDisplayCutoutSafeArea(captor.capture());
        assertEquals(48, captor.getValue().bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testClearPendingImeTransition_RestoresVisibilityWhenAwaitingDeferredFadeIn() {
        FrameLayout parent = attachResizingContainerToParent(1080, 2000);

        setupSmoothImeEnv(SheetState.FULL, 2000, 0, 80, 0);

        ResizeLock lock = mHelper.requestResize();
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        assertEquals(View.INVISIBLE, mView.getVisibility());
        ViewUtils.requestLayout(parent, "WebViewResizingHelperUnitTest");
        lock.unlock();

        // While awaiting deferred fade-in, reset is called.
        mHelper.reset();

        // View should be restored to visible and not left INVISIBLE.
        assertEquals(View.VISIBLE, mView.getVisibility());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
        ChromeFeatureList.GLIC_NO_WEBVIEW,
        ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
    })
    public void testPlaceholderCoordinator_UpdatesBottomInset_SmoothResize() {
        ResizingPlaceholderCoordinator coordinator = mock(ResizingPlaceholderCoordinator.class);
        doReturn(coordinator)
                .when(mMockComponentProvider)
                .createResizingPlaceholderCoordinator(any(), anyInt(), anyInt());
        when(mMockBottomSheetController.hasBottomInset()).thenReturn(true);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(48);
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mMockSheetContent);
        when(mMockSheetContent.getContentView()).thenReturn(mContainerView);
        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        when(mMockWebContents.getWidth()).thenReturn(ViewUtils.pxToDp(mContext, 500));
        when(mMockWebContents.getHeight()).thenReturn(ViewUtils.pxToDp(mContext, 800));

        FrameLayout parent = attachResizingContainerToParent(500, 800);
        mHelper.setThinWebView(mMockThinWebView, mMockWebContents);
        layoutViewAndAncestors(mHelper.getResizingContainer(), 500, 800);
        clearInvocations(mMockThinWebView, mMockWebContents);

        ResizeLock lock = mHelper.requestResize();
        assertNotNull(lock);
        verify(coordinator).updateBottomInset(48);

        ArgumentCaptor<WindowInsetObserver> insetObserverCaptor =
                ArgumentCaptor.forClass(WindowInsetObserver.class);
        verify(mMockInsetObserver).addObserver(insetObserverCaptor.capture());

        ArgumentCaptor<BottomSheetObserver> sheetObserverCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(sheetObserverCaptor.capture());

        // resizeWebContents suppression while locked
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // Changed bottom inset while ResizeLock held invokes updateBottomInset under smooth resize.
        clearInvocations(coordinator);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(72);
        insetObserverCaptor.getValue().onInsetChanged();
        verify(coordinator).updateBottomInset(72);

        // Container bottom margin change while ResizeLock held updates bottom inset.
        clearInvocations(coordinator);
        when(mMockBottomSheetController.getEdgeToEdgeBottomInset()).thenReturn(36);
        sheetObserverCaptor.getValue().onContainerBottomMarginChanged(100);
        verify(coordinator).updateBottomInset(36);

        // resizeWebContents remains suppressed while locked.
        verify(mMockThinWebView, never()).resizeWebContents(anyInt(), anyInt());

        // Reconciliation after lock.unlock()
        lock.unlock();
        verify(mMockThinWebView).resizeWebContents(500, 800);
    }
}
