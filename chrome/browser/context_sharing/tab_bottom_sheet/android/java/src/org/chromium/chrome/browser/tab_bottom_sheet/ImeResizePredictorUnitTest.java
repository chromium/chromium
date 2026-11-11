// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

/** Unit tests for {@link ImeResizePredictor}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_BOTTOM_SHEET,
    ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE,
    ChromeFeatureList.GLIC_NO_WEBVIEW,
    ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW,
    ChromeFeatureList.TAB_BOTTOM_SHEET_SMOOTH_KEYBOARD_RESIZE
})
public class ImeResizePredictorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockController;
    @Mock private InsetObserver mMockInsetObserver;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private ImeResizePredictor.Delegate mMockDelegate;

    private Context mContext;
    private View mMockView;
    private ImeResizePredictor mPredictor;

    private ImeResizePredictor createPredictor(
            @Nullable WindowAndroid windowAndroid,
            @Nullable InsetObserver insetObserver,
            @Nullable BottomSheetController controller) {
        return createPredictor(
                windowAndroid,
                insetObserver,
                controller,
                /* isSidePanel= */ false,
                /* webUiTopMarginPx= */ 0,
                mMockDelegate);
    }

    private ImeResizePredictor createPredictor(
            @Nullable WindowAndroid windowAndroid,
            @Nullable InsetObserver insetObserver,
            @Nullable BottomSheetController controller,
            boolean isSidePanel,
            @Px int webUiTopMarginPx,
            @Nullable ImeResizePredictor.Delegate delegate) {
        return new ImeResizePredictor(
                mMockView,
                mMockView,
                windowAndroid,
                insetObserver,
                controller,
                isSidePanel,
                webUiTopMarginPx,
                delegate);
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mMockView = new View(mContext);
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.RESUMED);
        when(mMockController.getSheetState()).thenReturn(SheetState.FULL);
        when(mMockController.getEdgeToEdgeBottomInset()).thenReturn(48);
        mPredictor = createPredictor(mMockWindowAndroid, mMockInsetObserver, mMockController);
    }

    @Test
    public void testOnPrepare_ValidatesImeAnimation() {
        var imeAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        assertTrue(mPredictor.onPrepare(imeAnim));

        var navAnim =
                new WindowInsetsAnimationCompat(
                        WindowInsetsCompat.Type.navigationBars(), null, 160);
        assertFalse(mPredictor.onPrepare(navAnim));
        assertFalse(mPredictor.onPrepare(null));
    }

    @Test
    public void testOnPrepare_NonImeOrInactiveActivity_Ignored() {
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);
        var imeAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        assertFalse(mPredictor.onPrepare(imeAnim));
    }

    @Test
    public void testOnPrepare_OverlayMode_Ignored() {
        when(mMockInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);
        var imeAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        assertFalse(mPredictor.onPrepare(imeAnim));
    }

    @Test
    public void testOnStart_DelegatesToBottomSheetController() {
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        int result = mPredictor.onStart(animation, bounds);

        assertEquals(1200, result);
        assertTrue(mPredictor.isPaused());
        assertTrue(mPredictor.isAnimationRunning());
        assertFalse(mPredictor.isClosingAnimation());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_EDGE_TO_EDGE)
    public void testOnStart_KeyboardOpens_NonEdgeToEdge_SubtractsTopMargin() {
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);

        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));

        var predictor =
                createPredictor(
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        /* webUiTopMarginPx= */ 100,
                        mMockDelegate);

        int result = predictor.onStart(animation, bounds);

        assertEquals(1100, result);
        assertTrue(predictor.isPaused());
        assertTrue(predictor.isAnimationRunning());
    }

    @Test
    public void testOnStart_ZeroBounds_Ignored() {
        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var zeroBounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 0));

        int result = mPredictor.onStart(animation, zeroBounds);

        assertEquals(0, result);
        assertFalse(mPredictor.isPaused());
        assertFalse(mPredictor.isAnimationRunning());
    }

    @Test
    public void testOnStart_ZeroSpanBounds_AbortsStart() {
        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var zeroSpanBounds = new BoundsCompat(Insets.of(0, 0, 0, 800), Insets.of(0, 0, 0, 800));

        int result = mPredictor.onStart(anim, zeroSpanBounds);

        assertEquals(0, result);
        assertFalse(mPredictor.isPaused());
        assertFalse(mPredictor.isAnimationRunning());
    }

    @Test
    public void testOnStart_NullBounds_Ignored() {
        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        int result = mPredictor.onStart(animation, null);
        assertEquals(0, result);
        assertFalse(mPredictor.isPaused());
        assertFalse(mPredictor.isAnimationRunning());
    }

    @Test
    public void testOnEnd_ClearsActiveAnimationsAndResetsPaused() {
        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);

        mPredictor.onStart(animation, bounds);
        assertTrue(mPredictor.isPaused());
        assertTrue(mPredictor.isAnimationRunning());

        boolean wasClosing = mPredictor.onEnd(animation, mMockView);
        assertFalse(wasClosing);
        assertFalse(mPredictor.isPaused());
        assertFalse(mPredictor.isAnimationRunning());

        // Non-IME animation onEnd should be ignored.
        var navAnim =
                new WindowInsetsAnimationCompat(
                        WindowInsetsCompat.Type.navigationBars(), null, 160);
        assertFalse(mPredictor.onEnd(navAnim, mMockView));
    }

    @Test
    public void testOnEnd_SupersededAnimation_Ignored() {
        var anim1 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);

        mPredictor.onStart(anim1, bounds);
        assertTrue(mPredictor.isAnimationRunning());

        // An onEnd for an older superseded animation should not clear the active transition.
        var anim2 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        assertFalse(mPredictor.onEnd(anim2, mMockView));
        assertTrue(mPredictor.isAnimationRunning());
    }

    @Test
    public void testClearActiveAnimations_ResetsState() {
        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);

        mPredictor.onStart(animation, bounds);
        assertTrue(mPredictor.isPaused());
        assertTrue(mPredictor.isAnimationRunning());

        mPredictor.clearActiveAnimations(mMockView);
        assertFalse(mPredictor.isPaused());
        assertFalse(mPredictor.isAnimationRunning());
    }

    @Test
    public void testReset_ClearsAllState() {
        var animation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);

        mPredictor.onStart(animation, bounds);
        mPredictor.reset();

        assertFalse(mPredictor.isPaused());
        assertFalse(mPredictor.isAnimationRunning());
        assertFalse(mPredictor.isKeyboardOpen());
    }

    @Test
    public void testClosingAnimation_TargetImeVisibleReturnsFalse() {
        WindowInsetsCompat insetsOpen =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);

        var openAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var openBounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);
        mPredictor.onStart(openAnim, openBounds);
        mPredictor.onEnd(openAnim, mMockView);
        assertTrue(mPredictor.isTargetImeVisible());

        var closeAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var closeBounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(2000);
        mPredictor.onPrepare(closeAnim);

        WindowInsetsCompat insetsDismissed =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsDismissed);

        mPredictor.onStart(closeAnim, closeBounds);
        assertTrue(mPredictor.isClosingAnimation());
        assertFalse(mPredictor.isTargetImeVisible());
    }

    @Test
    public void testDestroy_ResetsAllStateAndClearsDelegate() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        assertTrue(predictor.onPrepare(anim));

        predictor.destroy();

        assertFalse(predictor.isPaused());
        assertFalse(predictor.isAnimationRunning());
        assertFalse(predictor.isKeyboardOpen());

        assertFalse(predictor.reconcilePostImeBounds(/* ignoreCache= */ true));
        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());
        verify(mockDelegate).restoreVisibilityIfAwaitingFadeIn();
    }

    @Test
    public void testDelegate_OnStartAnimation_AppliesBoundsToDelegate() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1000);
        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 600));

        predictor.onStartAnimation(anim, bounds);

        verify(mockDelegate).updateBounds(eq(true), eq(1000), anyInt());
    }

    @Test
    public void testDelegate_OnSheetStateChanged_Hidden_ResetsState() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        assertTrue(predictor.onPrepare(anim));

        predictor.onSheetStateChanged(SheetState.HIDDEN);

        assertFalse(predictor.isPaused());
        assertFalse(predictor.isAnimationRunning());
    }

    @Test
    public void testSidePanel_BypassesBottomSheetController() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        null,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ true,
                        0,
                        mockDelegate);

        assertEquals(0, predictor.getProjectedWebUiHeight());
        assertEquals(
                ImeResizePredictor.Delegate.NO_SAFE_AREA_OVERRIDE,
                predictor.resolveTargetBottomSafeInsetPx());
    }

    @Test
    public void testOnStartAnimation_NonImeAnimation_IgnoredWithoutSchedulingWatchdog() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var navAnim =
                new WindowInsetsAnimationCompat(
                        WindowInsetsCompat.Type.navigationBars(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 100));

        predictor.onStartAnimation(navAnim, bounds);

        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());
        assertFalse(predictor.isAnimationRunning());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mockDelegate, never()).onPostResizeReconciled(anyBoolean());
    }

    @Test
    public void testOnStartAnimation_OverlayModeAfterPrepare_ClearsPendingTransition() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 600));

        predictor.onPrepareAnimation(anim);
        assertTrue(predictor.isAnimationRunning());

        // Overlay mode becomes active before onStartAnimation runs.
        when(mMockInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);
        predictor.onStartAnimation(anim, bounds);

        assertFalse(predictor.isAnimationRunning());
        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());
        verify(mockDelegate).restoreVisibilityIfAwaitingFadeIn();
    }

    @Test
    public void testOnEndAnimation_LayoutRequested_DefersReconciliation() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        View container = new View(mContext);
        ViewUtils.requestLayout(container, "ImeResizePredictorUnitTest");
        assertTrue(container.isLayoutRequested());

        var predictor =
                new ImeResizePredictor(
                        container,
                        container,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        predictor.onEndAnimation(anim);

        assertTrue(predictor.isPendingLayoutListenerAttached());
        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());

        container.layout(0, 0, 300, 400);
        container.getViewTreeObserver().dispatchOnGlobalLayout();

        assertFalse(predictor.isPendingLayoutListenerAttached());
        verify(mockDelegate)
                .updateBounds(
                        eq(false), eq(ImeResizePredictor.Delegate.NO_HEIGHT_OVERRIDE), anyInt());
    }

    @Test
    public void testOnEndAnimation_WhenOtherSheetShowing_ClearsPendingStateAndBails() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var otherContent = mock(BottomSheetContent.class);
        var otherView = new View(mContext);
        when(otherContent.getContentView()).thenReturn(otherView);
        when(mMockController.getCurrentSheetContent()).thenReturn(otherContent);

        var predictor =
                new ImeResizePredictor(
                        mMockView,
                        mMockView,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        predictor.onEndAnimation(anim);

        assertFalse(predictor.isAnimationRunning());
        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testOnEndAnimation_WhenOverlayModeOrInactiveActivity_ClearsPendingStateAndBails() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        when(mMockWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);

        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        predictor.onEndAnimation(anim);

        assertFalse(predictor.isAnimationRunning());
        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testOnActivityResumed_WhenOtherSheetShowingOrHidden_DoesNotPushBounds() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        when(mMockController.getSheetState()).thenReturn(SheetState.HIDDEN);

        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        predictor.onActivityResumed();

        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testOnEndAnimation_ClosingAnimation_ResolvesSafeAreaLazilyOnLayout() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        View container = new View(mContext);

        var predictor =
                new ImeResizePredictor(
                        container,
                        container,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        // Simulate open keyboard state.
        WindowInsetsCompat insetsOpen =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 800))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsOpen);
        var openAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var openBounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1200);
        predictor.onStart(openAnim, openBounds);
        predictor.onEnd(openAnim, container);

        // Now initiate close animation.
        var closeAnim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var closeBounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 800));
        predictor.onPrepare(closeAnim);
        WindowInsetsCompat insetsDismissed =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 0))
                        .build();
        when(mMockInsetObserver.getLastRawWindowInsets()).thenReturn(insetsDismissed);
        predictor.onStart(closeAnim, closeBounds);
        assertTrue(predictor.isClosingAnimation());

        // During IME hide, controller's inset supplier can be stale 0 before onApplyWindowInsets
        // lands.
        when(mMockController.getEdgeToEdgeBottomInset()).thenReturn(0);
        ViewUtils.requestLayout(container, "ImeResizePredictorUnitTest");
        predictor.onEndAnimation(closeAnim);
        assertTrue(predictor.isPendingLayoutListenerAttached());

        // Inset supplier is updated to 48px by the time onGlobalLayout runs.
        when(mMockController.getEdgeToEdgeBottomInset()).thenReturn(48);
        container.layout(0, 0, 300, 400);
        container.getViewTreeObserver().dispatchOnGlobalLayout();

        assertFalse(predictor.isPendingLayoutListenerAttached());
        verify(mockDelegate)
                .updateBounds(
                        eq(false), eq(ImeResizePredictor.Delegate.NO_HEIGHT_OVERRIDE), eq(48));
    }

    @Test
    public void testOnKeyboardInsetChanged_UpdatesSafeAreaInsetsWhenIdle() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        when(mMockController.getEdgeToEdgeBottomInset()).thenReturn(48);

        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        predictor.onKeyboardInsetChanged();

        verify(mockDelegate).updateSafeAreaInsets(eq(false), eq(48));
    }

    @Test
    public void testResolveTargetBottomSafeInsetPx_WithEdgeToEdge() {
        when(mMockController.getEdgeToEdgeBottomInset()).thenReturn(48);
        assertEquals(48, mPredictor.resolveTargetBottomSafeInsetPx());
    }

    @Test
    public void testOnActivityResumed_SidePanel_UpdatesBoundsWithoutSafeArea() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ true,
                        0,
                        mockDelegate);

        predictor.onActivityResumed();

        verify(mockDelegate)
                .updateBounds(
                        /* ignoreCache= */ true,
                        /* targetHeight= */ ImeResizePredictor.Delegate.NO_HEIGHT_OVERRIDE,
                        /* bottomSafeAreaInsetPx= */ ImeResizePredictor.Delegate
                                .NO_SAFE_AREA_OVERRIDE);
        verify(mockDelegate, never()).updateSafeAreaInsets(anyBoolean(), anyInt());
    }

    @Test
    public void testWatchdog_FiresAfterTimeoutAndReconciles() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1000);
        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 600));

        predictor.onStartAnimation(anim, bounds);
        assertTrue(predictor.isAnimationRunning());

        // Fast-forward shadow looper by 600ms watchdog timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(predictor.isAnimationRunning());
        verify(mockDelegate).onPostResizeReconciled(anyBoolean());
    }

    @Test
    public void testOnDisableResizingMode_WhenAnimationRunning_KeepsWatchdogArmed() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        when(mMockController.getProjectedContentHeightForIme()).thenReturn(1000);
        var anim = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        var bounds = new BoundsCompat(Insets.of(0, 0, 0, 0), Insets.of(0, 0, 0, 600));

        predictor.onStartAnimation(anim, bounds);
        assertTrue(predictor.isAnimationRunning());

        clearInvocations(mockDelegate);
        predictor.onDisableResizingMode();

        // Target height should be projected height (1000) and ignoreCache=true.
        verify(mockDelegate).updateBounds(eq(true), eq(1000), anyInt());
        // Watchdog remains armed while animation is running.
        assertTrue(predictor.isAnimationRunning());

        // When watchdog timeout elapses, the pending animation is cleanly resolved.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(predictor.isAnimationRunning());
    }

    @Test
    public void testOnDisableResizingMode_WhenIdle_CancelsWatchdog() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        var predictor =
                new ImeResizePredictor(
                        null,
                        null,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        predictor.onDisableResizingMode();

        verify(mockDelegate)
                .updateBounds(
                        eq(true), eq(ImeResizePredictor.Delegate.NO_HEIGHT_OVERRIDE), anyInt());
        assertFalse(predictor.isAnimationRunning());

        // Fast-forwarding looper produces no watchdog cleanup since it was cancelled.
        clearInvocations(mockDelegate);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyNoInteractions(mockDelegate);
    }

    @Test
    public void testOnDisableResizingMode_WhenLayoutPending_DefersReconciliation() {
        var mockDelegate = mock(ImeResizePredictor.Delegate.class);
        View container = new View(mContext);
        ViewUtils.requestLayout(container, "ImeResizePredictorUnitTest");
        assertTrue(container.isLayoutRequested());

        var predictor =
                new ImeResizePredictor(
                        container,
                        container,
                        mMockWindowAndroid,
                        mMockInsetObserver,
                        mMockController,
                        /* isSidePanel= */ false,
                        0,
                        mockDelegate);

        predictor.onDisableResizingMode();

        assertTrue(predictor.isPendingLayoutListenerAttached());
        verify(mockDelegate, never()).updateBounds(anyBoolean(), anyInt(), anyInt());

        container.layout(0, 0, 300, 400);
        container.getViewTreeObserver().dispatchOnGlobalLayout();

        assertFalse(predictor.isPendingLayoutListenerAttached());
        verify(mockDelegate)
                .updateBounds(
                        eq(true), eq(ImeResizePredictor.Delegate.NO_HEIGHT_OVERRIDE), anyInt());
    }
}
