// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.app.Activity;
import android.graphics.RectF;
import android.graphics.Shader;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.FakeSubscriptionTierBrandingDelegate;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for {@link AiTierPillDrawable}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class AiTierPillDrawableRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private FrameLayout mContentView;

    public AiTierPillDrawableRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        Activity activity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.setTheme(R.style.Theme_BrowserUI_DayNight);
                    mContentView = new FrameLayout(activity);
                    mContentView.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(activity));
                    activity.setContentView(mContentView);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPill() throws IOException {
        View pill = showPill(64, 22, new FakeSubscriptionTierBrandingDelegate());
        mRenderTestRule.render(pill, "ai_tier_pill");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTallPillIsFullyRounded() throws IOException {
        // E.g. with a large font size.
        View pill = showPill(96, 44, new FakeSubscriptionTierBrandingDelegate());
        mRenderTestRule.render(pill, "ai_tier_pill_tall");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPillWithoutWaveOrShadow() throws IOException {
        SubscriptionTierBrandingDelegate delegateWithoutPill =
                new SubscriptionTierBrandingDelegate() {
                    @Override
                    public Shader getRingShader(RectF bounds) {
                        return null;
                    }
                };
        View pill = showPill(64, 22, delegateWithoutPill);
        mRenderTestRule.render(pill, "ai_tier_pill_without_wave_or_shadow");
    }

    private View showPill(
            int widthDp, int heightDp, SubscriptionTierBrandingDelegate brandingDelegate) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    float density = activity.getResources().getDisplayMetrics().density;
                    View pill = new View(activity);
                    pill.setBackground(new AiTierPillDrawable(activity, brandingDelegate));
                    mContentView.addView(
                            pill,
                            new FrameLayout.LayoutParams(
                                    Math.round(widthDp * density),
                                    Math.round(heightDp * density),
                                    Gravity.CENTER));
                    return pill;
                });
    }
}
