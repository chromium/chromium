// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.animation.ObjectAnimator;
import android.animation.RectEvaluator;
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.Locale;

/** Render tests for {@link ShrinkExpandAnimator}. */
// TODO(crbug.com/40286625): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ShrinkExpandAnimatorRenderTest extends BlankUiTestActivityTestCase {
    private static final int ANIMATION_STEPS = 5;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    public ShrinkExpandAnimatorRenderTest() {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(false);
        mRenderTestRule.setNightModeEnabled(false);
    }

    private FrameLayout mRootView;
    private ShrinkExpandImageView mView;

    @Before
    public void setUp() throws Exception {
        Activity activity = getActivity();

        CallbackHelper onFirstLayout = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView = new FrameLayout(activity);
                    activity.setContentView(
                            mRootView,
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));

                    mView = new ShrinkExpandImageView(activity);
                    mRootView.addView(mView);
                    mView.runOnNextLayout(onFirstLayout::notifyCalled);
                });

        // Ensure layout has completed so getWidth() and getHeight() are non-zero.
        onFirstLayout.waitForOnly();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testExpandRect() throws Exception {
        Size thumbnailSize = getThumbnailSize();

        // Fullscreen
        Rect endValue = new Rect(0, 0, mRootView.getWidth(), mRootView.getHeight());

        // Center of screen
        int startX = Math.round(mRootView.getWidth() / 2.0f - thumbnailSize.getWidth() / 2.0f);
        int startY = Math.round(mRootView.getHeight() / 2.0f - thumbnailSize.getHeight() / 2.0f);
        Rect startValue =
                new Rect(
                        startX,
                        startY,
                        startX + thumbnailSize.getWidth(),
                        startY + thumbnailSize.getHeight());

        setupShrinkExpandImageView(startValue);
        ShrinkExpandAnimator animator = createAnimator(startValue, endValue, thumbnailSize);

        Rect startValueCopy = new Rect(startValue);
        Rect endValueCopy = new Rect(endValue);

        // Verify changing rects after doesn't cause ShrinkExpandAnimator to behave differently.
        startValue.left = 0;
        startValue.right = 10;
        startValue.top = 0;
        startValue.bottom = 10;
        endValue.left = 100;
        endValue.right = 200;
        endValue.top = 100;
        endValue.bottom = 200;

        stepThroughAnimation(
                "expand_rect", animator, startValueCopy, endValueCopy, ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testExpandRectWithTopClip() throws Exception {
        Size thumbnailSize = getThumbnailSize();

        // Fullscreen
        Rect endValue = new Rect(0, 0, mRootView.getWidth(), mRootView.getHeight());

        // Center top of screen
        int startX = Math.round(mRootView.getWidth() / 2.0f - thumbnailSize.getWidth() / 2.0f);
        Rect startValue =
                new Rect(
                        startX,
                        0,
                        startX + thumbnailSize.getWidth(),
                        Math.round(thumbnailSize.getHeight() / 2.0f));

        setupShrinkExpandImageView(startValue);
        ShrinkExpandAnimator animator = createAnimator(startValue, endValue, thumbnailSize);

        stepThroughAnimation(
                "expand_rect_with_top_clip", animator, startValue, endValue, ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testShrinkRect() throws Exception {
        Size thumbnailSize = getThumbnailSize();

        // Fullscreen
        Rect startValue = new Rect(0, 0, mRootView.getWidth(), mRootView.getHeight());

        // Center of screen
        int endX = Math.round(mRootView.getWidth() / 2.0f - thumbnailSize.getWidth() / 2.0f);
        int endY = Math.round(mRootView.getHeight() / 2.0f - thumbnailSize.getHeight() / 2.0f);
        Rect endValue =
                new Rect(
                        endX,
                        endY,
                        endX + thumbnailSize.getWidth(),
                        endY + thumbnailSize.getHeight());

        setupShrinkExpandImageView(startValue);
        ShrinkExpandAnimator animator = createAnimator(startValue, endValue, thumbnailSize);

        stepThroughAnimation("shrink_rect_rect", animator, startValue, endValue, ANIMATION_STEPS);
    }

    private ShrinkExpandAnimator createAnimator(
            Rect startValue, Rect endValue, @Nullable Size thumbnailSize) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShrinkExpandAnimator animator =
                            new ShrinkExpandAnimator(mView, startValue, endValue);
                    animator.setThumbnailSizeForOffset(thumbnailSize);
                    animator.setRect(startValue);
                    return animator;
                });
    }

    /** Returns a thumbnail size 1/4 the size of {@link mRootView}. */
    private Size getThumbnailSize() {
        return new Size(
                Math.round(mRootView.getWidth() / 4.0f), Math.round(mRootView.getHeight() / 4.0f));
    }

    /**
     * Steps through an animation.
     *
     * @param testcaseName The base name for the render test results.
     * @param rectAnimator The animator to drive the animation of.
     * @param startValue The initial react.
     * @param endValue The final rect.
     * @param steps The number of steps to take. Must be 2 or more.
     */
    private void stepThroughAnimation(
            String testcaseName,
            ShrinkExpandAnimator rectAnimator,
            Rect startValue,
            Rect endValue,
            int steps)
            throws Exception {
        assert steps >= 2;
        float fractionPerStep = 1.0f / (steps - 1);

        ObjectAnimator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return ObjectAnimator.ofObject(
                                    rectAnimator,
                                    ShrinkExpandAnimator.RECT,
                                    new RectEvaluator(),
                                    startValue,
                                    endValue);
                        });

        // Manually drive the animation instead of using an ObjectAnimator for exact control over
        // step size and timing.
        for (int step = 0; step < steps; step++) {
            final float animationFraction = fractionPerStep * step;
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        animator.setCurrentFraction(animationFraction);
                    });

            mRenderTestRule.render(
                    mRootView,
                    testcaseName
                            + String.format(Locale.ENGLISH, "_step_%d_of_%d", step + 1, steps));
        }
    }

    /**
     * Sets the initial position of the image view.
     *
     * @param startValue The rect to position the image view at.
     */
    private void setupShrinkExpandImageView(Rect startValue) throws Exception {
        CallbackHelper onNextLayout = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout.LayoutParams layoutParams =
                            (FrameLayout.LayoutParams) mView.getLayoutParams();
                    layoutParams.width = startValue.width();
                    layoutParams.height = startValue.height();
                    layoutParams.setMargins(startValue.left, startValue.top, 0, 0);
                    mView.setLayoutParams(layoutParams);
                    mView.setImageBitmap(createBitmap());
                    mView.setScaleX(1.0f);
                    mView.setScaleY(1.0f);
                    mView.setTranslationX(0.0f);
                    mView.setTranslationY(0.0f);
                    mView.setVisibility(View.VISIBLE);
                    mView.runOnNextLayout(onNextLayout::notifyCalled);
                });

        // Wait for a layout to make sure the ShrinkExpandImageView is positioned correctly before
        // starting to step through the animation.
        onNextLayout.waitForNext();
    }

    /** Returns a blue checkerboard bitmap the size of {@link mRootView}. */
    private Bitmap createBitmap() {
        int width = mRootView.getWidth();
        int height = mRootView.getHeight();

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.DITHER_FLAG);
        paint.setColor(Color.BLUE);
        int rectSizePx = 100;
        int stepPx = rectSizePx * 2;
        for (int y = 0; y < height; y += stepPx) {
            for (int x = 0; x < width; x += stepPx) {
                canvas.drawRect(x, y, x + rectSizePx, y + rectSizePx, paint);
            }
        }
        for (int y = rectSizePx; y < height; y += stepPx) {
            for (int x = rectSizePx; x < width; x += stepPx) {
                canvas.drawRect(x, y, x + rectSizePx, y + rectSizePx, paint);
            }
        }
        return bitmap;
    }
}
