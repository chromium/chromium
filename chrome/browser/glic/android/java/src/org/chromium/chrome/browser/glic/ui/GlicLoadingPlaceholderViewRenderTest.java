// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render tests for {@link GlicLoadingPlaceholderView}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
public class GlicLoadingPlaceholderViewRenderTest {
    // Mirror kColorGlicBackground and kColorGlicPlaceholderElem in
    // chrome/browser/ui/color/chrome_color_mixer.cc.
    private static final int BACKGROUND_COLOR_LIGHT = 0xFFFDFCFC;
    private static final int BACKGROUND_COLOR_DARK = 0xFF000000;
    private static final int PLACEHOLDER_ELEM_COLOR_LIGHT = 0xFFEFF3FA;
    private static final int PLACEHOLDER_ELEM_COLOR_DARK = 0xFF333438;

    // Tall enough for the top body block to reach its max height, and short enough to shrink it.
    private static final int TALL_HEIGHT_DP = 800;
    private static final int SHORT_HEIGHT_DP = 400;

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_GLIC)
                    .setRevision(0)
                    .setDescription("Glic native loading skeleton")
                    .build();

    @Rule public final MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    private boolean mNightModeEnabled;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setUpNightMode(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mNightModeEnabled = nightModeEnabled;
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testShort(boolean nightModeEnabled) throws IOException {
        renderPlaceholder(SHORT_HEIGHT_DP, "glic_loading_placeholder_short");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTall() throws IOException {
        renderPlaceholder(TALL_HEIGHT_DP, "glic_loading_placeholder_tall");
    }

    private void renderPlaceholder(int heightDp, String renderId) throws IOException {
        GlicLoadingPlaceholderView placeholder =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Activity activity = mActivityTestRule.getActivity();
                            FrameLayout root = new FrameLayout(activity);
                            activity.setContentView(
                                    root,
                                    new ViewGroup.LayoutParams(
                                            MATCH_PARENT, ViewUtils.dpToPx(activity, heightDp)));
                            GlicLoadingPlaceholderView view =
                                    GlicLoadingPlaceholderView.create(
                                            activity,
                                            mNightModeEnabled
                                                    ? BACKGROUND_COLOR_DARK
                                                    : BACKGROUND_COLOR_LIGHT,
                                            mNightModeEnabled
                                                    ? PLACEHOLDER_ELEM_COLOR_DARK
                                                    : PLACEHOLDER_ELEM_COLOR_LIGHT,
                                            CallbackUtils.emptyRunnable());
                            root.addView(
                                    view, new FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT));
                            return view;
                        });

        // Animations are disabled in instrumentation tests, so the blocks stay at full opacity.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(placeholder.getWidth(), Matchers.greaterThan(0));
                    Criteria.checkThat(placeholder.isLayoutRequested(), Matchers.is(false));
                    Criteria.checkThat(placeholder.isAnimatingForTesting(), Matchers.is(false));
                });

        mRenderTestRule.render(placeholder, renderId);
    }
}
