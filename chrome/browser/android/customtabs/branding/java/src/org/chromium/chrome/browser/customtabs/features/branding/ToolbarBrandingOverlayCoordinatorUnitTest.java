// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayCoordinator.HIDING_DURATION_MS;
import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.HIDING_PROGRESS;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.core.animation.AnimatorTestRule;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ToolbarBrandingOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarBrandingOverlayCoordinatorUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public AnimatorTestRule mAnimatorTestRule = new AnimatorTestRule();

    private Activity mActivity;
    private ToolbarBrandingOverlayCoordinator mCoordinator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            FrameLayout content = new FrameLayout(mActivity);
                            ViewStub stub = new ViewStub(mActivity);
                            stub.setLayoutResource(R.layout.custom_tabs_toolbar_branding_layout);
                            stub.setInflatedId(R.id.branding_layout);
                            content.addView(stub);
                            mActivity.setContentView(content);
                            mModel =
                                    new PropertyModel.Builder(
                                                    ToolbarBrandingOverlayProperties.ALL_KEYS)
                                            .with(
                                                    ToolbarBrandingOverlayProperties.COLOR_DATA,
                                                    new ToolbarBrandingOverlayProperties.ColorData(
                                                            Color.WHITE,
                                                            BrandedColorScheme.APP_DEFAULT))
                                            .build();
                            mCoordinator = new ToolbarBrandingOverlayCoordinator(stub, mModel);
                        });
    }

    @Test
    public void testAnimateHide() {
        assertNotNull(mActivity.findViewById(R.id.branding_layout));

        long startTime = mAnimatorTestRule.getCurrentTime();
        mCoordinator.hideAndDestroy();

        float prevProgress = mModel.get(HIDING_PROGRESS);
        while (mAnimatorTestRule.getCurrentTime() - startTime < HIDING_DURATION_MS) {
            mAnimatorTestRule.advanceTimeBy(25);
            float newProgress = mModel.get(HIDING_PROGRESS);
            assertThat(
                    "Animation should be progressing as time passes.",
                    newProgress,
                    greaterThan(prevProgress));
            prevProgress = newProgress;
        }

        assertNull(mActivity.findViewById(R.id.branding_layout));
    }

    @Test
    public void testDestroyBeforeHide() {
        mCoordinator.destroy();

        assertNull(mActivity.findViewById(R.id.branding_layout));
    }

    @Test
    public void testDestroyDuringHide() {
        mCoordinator.hideAndDestroy();
        mAnimatorTestRule.advanceTimeBy(HIDING_DURATION_MS / 2);
        mCoordinator.destroy();

        assertNull(mActivity.findViewById(R.id.branding_layout));
    }

    @Test
    public void testDestroyAfterHide() {
        mCoordinator.hideAndDestroy();
        mAnimatorTestRule.advanceTimeBy(HIDING_DURATION_MS + 10);
        mCoordinator.destroy();

        assertNull(mActivity.findViewById(R.id.branding_layout));
    }
}
