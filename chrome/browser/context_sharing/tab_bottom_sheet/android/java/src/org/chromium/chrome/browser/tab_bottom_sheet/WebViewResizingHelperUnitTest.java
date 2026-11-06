// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
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

import org.chromium.base.ActivityState;
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

/** Unit tests for {@link WebViewResizingHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebViewResizingHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ThinWebView mMockThinWebView;
    @Mock private WebContents mMockWebContents;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private DisplayAndroid mMockDisplayAndroid;
    @Mock private InsetObserver mMockInsetObserver;
    @Mock private Window mMockWindow;
    @Mock private CoBrowseComponentProvider mMockComponentProvider;
    @Captor private ArgumentCaptor<WindowInsetsAnimationListener> mAnimationListenerCaptor;

    private Context mContext;
    private View mMockDecorView;
    private View mView;
    private View mContainerView;
    private WebViewResizingHelper mHelper;
    private final SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier =
            ObservableSuppliers.createNonNull(0);

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
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
}
