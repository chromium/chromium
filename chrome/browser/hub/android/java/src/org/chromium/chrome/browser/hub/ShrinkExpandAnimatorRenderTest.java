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

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.ui.animation.RenderTestAnimationUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for {@link ShrinkExpandAnimator}. */
// TODO(crbug.com/40286625): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ShrinkExpandAnimatorRenderTest {
    private static final int ANIMATION_STEPS = 5;
    private static final int SEARCH_BOX_HEIGHT = 50;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .setRevision(2)
                    .build();

    private FrameLayout mRootView;
    private ShrinkExpandImageView mView;

    // Retains a strong reference to the {@link ShrinkExpandAnimator} on the class to prevent it
    // from being prematurely GC'd during {@link RenderTestAnimationUtils#stepThroughAnimation}. The
    // {@link ObjectAnimator} only retains a {@link java.lang.ref.WeakReference} to the object,
    //  meaning it could be GC'd anytime after changing stack frames.
    private ShrinkExpandAnimator mAnimator;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(false);
        mRenderTestRule.setNightModeEnabled(false);

        CallbackHelper onFirstLayout = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRootView = new FrameLayout(sActivity);
                    sActivity.setContentView(
                            mRootView,
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));

                    mView = new ShrinkExpandImageView(sActivity);
                    mRootView.addView(mView);
                    mView.runOnNextLayout(onFirstLayout::notifyCalled);
                });

        // Ensure layout has completed so getWidth() and getHeight() are non-zero.
        onFirstLayout.waitForOnly();
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
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

        Rect startValueCopy = new Rect(startValue);
        Rect endValueCopy = new Rect(endValue);

        setupShrinkExpandImageView(startValue);
        mAnimator = createAnimator(startValue, endValue, thumbnailSize, /* searchBoxHeight= */ 0);
        ObjectAnimator expandAnimator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ObjectAnimator.ofObject(
                                        mAnimator,
                                        ShrinkExpandAnimator.RECT,
                                        new RectEvaluator(),
                                        startValueCopy,
                                        endValueCopy));

        // Verify changing rects after doesn't cause ShrinkExpandAnimator to behave differently.
        startValue.left = 0;
        startValue.right = 10;
        startValue.top = 0;
        startValue.bottom = 10;
        endValue.left = 100;
        endValue.right = 200;
        endValue.top = 100;
        endValue.bottom = 200;

        RenderTestAnimationUtils.stepThroughAnimation(
                "expand_rect", mRenderTestRule, mRootView, expandAnimator, ANIMATION_STEPS);
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
        mAnimator = createAnimator(startValue, endValue, thumbnailSize, /* searchBoxHeight= */ 0);
        ObjectAnimator expandAnimator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ObjectAnimator.ofObject(
                                        mAnimator,
                                        ShrinkExpandAnimator.RECT,
                                        new RectEvaluator(),
                                        startValue,
                                        endValue));

        RenderTestAnimationUtils.stepThroughAnimation(
                "expand_rect_with_top_clip",
                mRenderTestRule,
                mRootView,
                expandAnimator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testExpandRectWithTopClip_hubSearchBoxAdjustment() throws Exception {
        Size thumbnailSize = getThumbnailSize();

        // Fullscreen
        Rect endValue =
                new Rect(0, -SEARCH_BOX_HEIGHT, mRootView.getWidth(), mRootView.getHeight());

        // Center top of screen
        int startX = Math.round(mRootView.getWidth() / 2.0f - thumbnailSize.getWidth() / 2.0f);
        Rect startValue =
                new Rect(
                        startX,
                        0,
                        startX + thumbnailSize.getWidth(),
                        Math.round(thumbnailSize.getHeight() / 2.0f));

        setupShrinkExpandImageView(startValue);
        mAnimator = createAnimator(startValue, endValue, thumbnailSize, SEARCH_BOX_HEIGHT);
        ObjectAnimator expandAnimator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ObjectAnimator.ofObject(
                                        mAnimator,
                                        ShrinkExpandAnimator.RECT,
                                        new RectEvaluator(),
                                        startValue,
                                        endValue));

        RenderTestAnimationUtils.stepThroughAnimation(
                "expand_rect_with_top_clip_hub_search",
                mRenderTestRule,
                mRootView,
                expandAnimator,
                ANIMATION_STEPS);
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
        mAnimator = createAnimator(startValue, endValue, thumbnailSize, /* searchBoxHeight= */ 0);
        ObjectAnimator shrinkAnimator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ObjectAnimator.ofObject(
                                        mAnimator,
                                        ShrinkExpandAnimator.RECT,
                                        new RectEvaluator(),
                                        startValue,
                                        endValue));

        RenderTestAnimationUtils.stepThroughAnimation(
                "shrink_rect_rect", mRenderTestRule, mRootView, shrinkAnimator, ANIMATION_STEPS);
    }

    private ShrinkExpandAnimator createAnimator(
            Rect startValue, Rect endValue, @Nullable Size thumbnailSize, int searchBoxHeight) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShrinkExpandAnimator animator =
                            new ShrinkExpandAnimator(mView, startValue, endValue, searchBoxHeight);
                    animator.setThumbnailSizeForOffset(thumbnailSize);
                    animator.setRect(startValue);
                    return animator;
                });
    }

    /** Returns a thumbnail size 1/4 the size of {@link #mRootView}. */
    private Size getThumbnailSize() {
        return new Size(
                Math.round(mRootView.getWidth() / 4.0f), Math.round(mRootView.getHeight() / 4.0f));
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

    /** Returns a blue checkerboard bitmap the size of {@link #mRootView}. */
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
