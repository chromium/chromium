// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/** Render tests for {@link AutoPipPermissionDialogView}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class AutoPipPermissionDialogViewRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private final @ColorInt int mFakeBgColor;
    private FrameLayout mContentView;
    private AutoPipPermissionDialogView mView;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_MESSAGES)
                    .build();

    public AutoPipPermissionDialogViewRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    // This helper function waits until the view is rendered trying to prevent flakiness.
    private void waitForViewToBeRendered(View view) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return view.isShown()
                            && view.getWidth() > 0
                            && view.getHeight() > 0
                            // Ensure that there are no pending layout passes, which could mean the
                            // view is in an intermediate state.
                            && !view.isLayoutRequested();
                },
                "View not rendered: " + view.getClass().getSimpleName());
    }

    @After
    public void tearDown() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private void setUpViews() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = sActivity;
                    mContentView = new FrameLayout(activity);
                    mView =
                            new AutoPipPermissionDialogView(
                                    activity,
                                    "Allow while visiting the site",
                                    "Allow this time",
                                    "Don't allow",
                                    (result) -> {});
                    mContentView.setBackgroundColor(mFakeBgColor);
                    activity.setContentView(mContentView);
                    mContentView.addView(mView, MATCH_PARENT, WRAP_CONTENT);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_Default() throws IOException {
        setUpViews();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setOrigin("example.com");
                });
        waitForViewToBeRendered(mView);
        mRenderTestRule.render(mView, "auto_pip_permission_dialog_default");
    }
}
