// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.widget.ImageViewCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.GraphicsMode;
import org.robolectric.shadows.ShadowChoreographer;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.glic.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;

import java.time.Duration;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link GlicLoadingPlaceholderView}. */
@RunWith(BaseRobolectricTestRunner.class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class GlicLoadingPlaceholderViewUnitTest {
    private static final float EPSILON = 0.001f;
    private static final int TEST_BG_COLOR = 0xFF1F1F1F;
    private static final int TEST_ELEM_COLOR = 0xFF3A3A3A;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    private Context mContext;
    private float mOriginalDurationScale;

    @Before
    public void setUp() {
        mOriginalDurationScale = ValueAnimator.getDurationScale();
        ValueAnimator.setDurationScale(1.0f);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        ValueAnimator.setDurationScale(mOriginalDurationScale);
    }

    private GlicLoadingPlaceholderView createView(Context context, Runnable onCloseClicked) {
        return GlicLoadingPlaceholderView.create(
                context, TEST_BG_COLOR, TEST_ELEM_COLOR, onCloseClicked);
    }

    @Test
    public void testLayoutColorsAndCloseButtonTint() {
        GlicLoadingPlaceholderView view = createView(mContext, CallbackUtils.emptyRunnable());

        assertTrue(view.getBackground() instanceof ColorDrawable);
        assertEquals(TEST_BG_COLOR, ((ColorDrawable) view.getBackground()).getColor());

        List<View> blocks = view.getPlaceholderBlocksForTesting();
        assertEquals(4, blocks.size());
        for (View block : blocks) {
            assertTrue(block.getBackground() instanceof GradientDrawable);
            GradientDrawable drawable = (GradientDrawable) block.getBackground();
            assertNotNull(drawable.getColor());
            assertEquals(TEST_ELEM_COLOR, drawable.getColor().getDefaultColor());
            assertTrue(drawable.getCornerRadius() > 0f);
        }

        assertEquals(View.VISIBLE, view.getCloseButtonForTesting().getVisibility());
        assertEquals(
                mContext.getString(R.string.close),
                view.getCloseButtonForTesting().getContentDescription().toString());
        assertNotNull(ImageViewCompat.getImageTintList(view.getCloseButtonForTesting()));
    }

    @Test
    public void testAccessibilityExposesOnlyCloseButton() {
        GlicLoadingPlaceholderView view = createView(mContext, CallbackUtils.emptyRunnable());

        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, view.getImportantForAccessibility());
        for (View block : view.getPlaceholderBlocksForTesting()) {
            assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, block.getImportantForAccessibility());
        }

        View closeButton = view.getCloseButtonForTesting();
        assertTrue(closeButton.isFocusable());
        assertTrue(closeButton.isImportantForAccessibility());
        assertEquals(
                mContext.getString(R.string.close), closeButton.getContentDescription().toString());
    }

    @Test
    public void testCloseButtonClickInvokesCallback() {
        AtomicInteger closeClicks = new AtomicInteger();
        GlicLoadingPlaceholderView view = createView(mContext, closeClicks::incrementAndGet);

        assertTrue(view.getCloseButtonForTesting().performClick());
        assertEquals(1, closeClicks.get());
    }

    @Test
    public void testCloseButtonMeetsMinTouchTarget() {
        GlicLoadingPlaceholderView view = createView(mContext, CallbackUtils.emptyRunnable());
        int touchTargetPx = dimenPx(R.dimen.min_touch_target_size);
        int marginEndPx = dimenPx(R.dimen.glic_loading_placeholder_close_button_margin_end);
        for (int widthDp : new int[] {400, 320}) {
            int widthPx = dpToPx(widthDp);
            measureAndLayout(view, widthPx, dpToPx(700));
            View closeButton = view.getCloseButtonForTesting();
            View headerEnd = view.getPlaceholderBlocksForTesting().get(1);
            assertEquals(touchTargetPx, closeButton.getWidth());
            assertEquals(touchTargetPx, closeButton.getHeight());
            assertEquals(marginEndPx, widthPx - closeButton.getRight());
            assertTrue(headerEnd.getRight() <= closeButton.getLeft());
        }
    }

    @Test
    public void testShortHeightClampsBodyUpperBlockWithoutClippingBottomBlock() {
        GlicLoadingPlaceholderView view = createView(mContext, CallbackUtils.emptyRunnable());
        List<View> blocks = view.getPlaceholderBlocksForTesting();
        int bodyTopMaxPx = dimenPx(R.dimen.glic_loading_placeholder_body_top_max_height);
        int bodyBottomPx = dimenPx(R.dimen.glic_loading_placeholder_body_bottom_height);

        int widthPx = dpToPx(400);
        measureAndLayout(view, widthPx, dpToPx(700));
        assertEquals(bodyTopMaxPx, blocks.get(2).getHeight());
        assertEquals(bodyBottomPx, blocks.get(3).getHeight());

        measureAndLayout(view, widthPx, dpToPx(240));
        assertTrue(blocks.get(2).getHeight() < bodyTopMaxPx);
        assertEquals(bodyBottomPx, blocks.get(3).getHeight());
    }

    @Test
    public void testPulseAnimationVisibilityLifecycle() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        GlicLoadingPlaceholderView view = createView(activity, CallbackUtils.emptyRunnable());

        // Before attaching to a window, animation is inactive.
        assertFalse(view.isAnimatingForTesting());

        // Pause Choreographer vsync auto-advancement so advancing a single 16ms frame flushes
        // ViewRootImpl window-visibility traversals without running ShadowValueAnimator's
        // single-repeat stand-in for ValueAnimator.INFINITE (750ms half-cycle) to completion.
        ShadowChoreographer.setPaused(true);
        try {
            // Attaching while visible starts the pulse animation once window traversal completes.
            activity.setContentView(view);
            advanceOneFrame();
            assertTrue(view.isAnimatingForTesting());

            // Hiding stops animation and resets alphas.
            setBlockAlphas(view, GlicLoadingPlaceholderView.LOW_OPACITY);
            view.setVisibility(View.GONE);
            assertFalse(view.isAnimatingForTesting());
            assertBlockAlphasReset(view);

            // Making visible again restarts animation; hiding the host window stops it.
            view.setVisibility(View.VISIBLE);
            assertTrue(view.isAnimatingForTesting());
            setBlockAlphas(view, GlicLoadingPlaceholderView.LOW_OPACITY);
            activity.getWindow().getDecorView().setVisibility(View.GONE);
            advanceOneFrame();
            assertFalse(view.isAnimatingForTesting());
            assertBlockAlphasReset(view);

            // Restoring window visibility resumes animation; detaching stops it.
            activity.getWindow().getDecorView().setVisibility(View.VISIBLE);
            advanceOneFrame();
            assertTrue(view.isAnimatingForTesting());
            setBlockAlphas(view, GlicLoadingPlaceholderView.LOW_OPACITY);
            ((ViewGroup) view.getParent()).removeView(view);
            advanceOneFrame();
            assertFalse(view.isAnimatingForTesting());
            assertBlockAlphasReset(view);
        } finally {
            ShadowChoreographer.setPaused(false);
        }
    }

    @Test
    public void testComponentProvidersCreateGlicLoadingPlaceholderView() {
        AtomicInteger closeClicks = new AtomicInteger();

        View bottomSheetPlaceholder =
                new GlicBottomSheetComponentProvider(mProfile, /* nativeCoordinator= */ null)
                        .createLoadingPlaceholderView(
                                mContext,
                                TEST_BG_COLOR,
                                TEST_ELEM_COLOR,
                                closeClicks::incrementAndGet);
        assertTrue(bottomSheetPlaceholder instanceof GlicLoadingPlaceholderView);
        ((GlicLoadingPlaceholderView) bottomSheetPlaceholder)
                .getCloseButtonForTesting()
                .performClick();
        assertEquals(1, closeClicks.get());

        View sidePanelPlaceholder =
                new GlicSidePanelComponentProvider()
                        .createLoadingPlaceholderView(
                                mContext,
                                TEST_BG_COLOR,
                                TEST_ELEM_COLOR,
                                closeClicks::incrementAndGet);
        assertTrue(sidePanelPlaceholder instanceof GlicLoadingPlaceholderView);
        ((GlicLoadingPlaceholderView) sidePanelPlaceholder)
                .getCloseButtonForTesting()
                .performClick();
        assertEquals(2, closeClicks.get());
    }

    private static void setBlockAlphas(GlicLoadingPlaceholderView view, float alpha) {
        for (View block : view.getPlaceholderBlocksForTesting()) {
            block.setAlpha(alpha);
        }
    }

    private static void assertBlockAlphasReset(GlicLoadingPlaceholderView view) {
        for (View block : view.getPlaceholderBlocksForTesting()) {
            assertEquals(GlicLoadingPlaceholderView.HIGH_OPACITY, block.getAlpha(), EPSILON);
        }
    }

    private static void advanceOneFrame() {
        ShadowSystemClock.advanceBy(Duration.ofMillis(16));
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private static void measureAndLayout(View view, int widthPx, int heightPx) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(widthPx, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(heightPx, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, widthPx, heightPx);
    }

    private int dpToPx(float dp) {
        return ViewUtils.dpToPx(mContext, dp);
    }

    private int dimenPx(int dimenRes) {
        return mContext.getResources().getDimensionPixelSize(dimenRes);
    }
}
