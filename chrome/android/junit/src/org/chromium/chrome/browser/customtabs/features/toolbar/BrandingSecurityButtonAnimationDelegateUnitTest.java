// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.graphics.drawable.VectorDrawable;
import android.os.Looper;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.DrawableRes;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowDrawable;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link CustomTabToolbarAnimationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class, ShadowDrawable.class})
@LooperMode(Mode.PAUSED)
public class BrandingSecurityButtonAnimationDelegateUnitTest {
    private static final @DrawableRes int ICON_16_DP = R.drawable.ic_group_icon_16dp;
    private static final @DrawableRes int ICON_24_DP = R.drawable.ic_globe_24dp;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Spy ImageButton mImageButton;
    Activity mActivity;

    private BrandingSecurityButtonAnimationDelegate mAnimationDelegate;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);

        LinearLayout content = new LinearLayout(mActivity, null);
        content.setOrientation(LinearLayout.HORIZONTAL);
        mActivity.setContentView(content);

        mImageButton = Mockito.spy(new ImageButton(mActivity, null));
        content.addView(mImageButton);

        mAnimationDelegate = new BrandingSecurityButtonAnimationDelegate(mImageButton);
    }

    @Test
    public void testTransition_Invisible() {
        // There's no great way to test the animation. This test case focuses on checking the
        // drawable when transition start -> ongoing -> finish.
        setupInitialImageButtonState();

        mImageButton.setVisibility(View.INVISIBLE);
        mAnimationDelegate.updateDrawableResource(ICON_24_DP);
        assertDrawableResource(ICON_24_DP, mImageButton.getDrawable());
    }

    @Test
    public void testTransition_Rotation() {
        // There's no great way to test the animation. This test case focuses on checking the
        // drawable when transition start -> ongoing -> finish.
        setupInitialImageButtonState();

        mAnimationDelegate.updateDrawableResource(ICON_24_DP);
        Assert.assertTrue(
                "Drawable should be a TransitionDrawable during animation.",
                mImageButton.getDrawable() instanceof TransitionDrawable);

        // Advance looper so the animation finishes.
        advanceLooper(BrandingSecurityButtonAnimationDelegate.ICON_ANIMATION_DURATION_MS);
        assertDrawableResource(ICON_24_DP, mImageButton.getDrawable());
        verify(mImageButton, atLeastOnce()).setRotation(anyFloat());
        Assert.assertEquals("Rotation should be reset.", 0, mImageButton.getRotation(), 0.01f);
    }

    @Test
    public void testTransition_Rotation_UpdateAgainDuringTransition() {
        // There's no great way to test the animation. This test case focuses on checking the
        // drawable when transition start -> ongoing -> finish.
        setupInitialImageButtonState();

        mAnimationDelegate.updateDrawableResource(ICON_24_DP);
        Assert.assertTrue(
                "Drawable should be a TransitionDrawable during animation.",
                mImageButton.getDrawable() instanceof TransitionDrawable);

        // Advance half way through the animation.
        advanceLooper(BrandingSecurityButtonAnimationDelegate.ICON_ANIMATION_DURATION_MS / 2);

        // Start another transition back to the original icon.
        mAnimationDelegate.updateDrawableResource(ICON_16_DP);
        advanceLooper(BrandingSecurityButtonAnimationDelegate.ICON_ANIMATION_DURATION_MS);
        assertDrawableResource(ICON_16_DP, mImageButton.getDrawable());
        verify(mImageButton, atLeastOnce()).setRotation(anyFloat());
        Assert.assertEquals("Rotation should be reset.", 0, mImageButton.getRotation(), 0.01f);
    }

    @Test
    public void testResizeToBitmapDrawable() {
        Resources resources = mActivity.getResources();
        Drawable drawable = ApiCompatibilityUtils.getDrawable(resources, ICON_24_DP);
        Assert.assertTrue(drawable instanceof VectorDrawable);

        int width = drawable.getIntrinsicWidth();
        int height = drawable.getIntrinsicHeight();

        BitmapDrawable d1 =
                BrandingSecurityButtonAnimationDelegate.resizeToBitmapDrawable(
                        resources, drawable, width, height);
        Assert.assertEquals(
                "Width of the resized drawable is different.", width, d1.getIntrinsicWidth());
        Assert.assertEquals(
                "Height of the resized drawable is different.", height, d1.getIntrinsicHeight());

        BitmapDrawable d2 =
                BrandingSecurityButtonAnimationDelegate.resizeToBitmapDrawable(
                        resources, drawable, width * 2, height * 2);
        Assert.assertEquals(
                "Width of the resized drawable is different.", width * 2L, d2.getIntrinsicWidth());
        Assert.assertEquals(
                "Height of the resized drawable is different.",
                height * 2L,
                d2.getIntrinsicHeight());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testResizeDrawable_InvalidInput() {
        Resources resources = mActivity.getResources();
        Drawable drawable = ApiCompatibilityUtils.getDrawable(resources, ICON_24_DP);
        Assert.assertTrue(drawable instanceof VectorDrawable);

        int width = drawable.getIntrinsicWidth();
        int height = drawable.getIntrinsicHeight();

        BitmapDrawable d3 =
                BrandingSecurityButtonAnimationDelegate.resizeToBitmapDrawable(
                        resources, drawable, width - 1, height - 1);
    }

    private void setupInitialImageButtonState() {
        mAnimationDelegate.updateDrawableResource(ICON_16_DP);
        // No transition is added for the first drawable.
        assertDrawableResource(ICON_16_DP, mImageButton.getDrawable());
        mImageButton.setVisibility(View.VISIBLE);
    }

    private void advanceLooper(long durationMs) {
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(durationMs, TimeUnit.MILLISECONDS);
    }

    private void assertDrawableResource(@DrawableRes int drawableRes, Drawable drawable) {
        ShadowDrawable shadowDrawable = Shadows.shadowOf(drawable);
        Assert.assertEquals(
                "Drawable resource is not equal.",
                drawableRes,
                shadowDrawable.getCreatedFromResId());
    }
}
