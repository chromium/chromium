// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;

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
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render tests for the Glic microphone permission dialog. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class GlicMicPermissionDialogRenderTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_GLIC)
                    .setRevision(0)
                    .build();

    @Rule public final MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    private BlankUiTestActivity mActivity;
    private ModalDialogManager mModalDialogManager;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setUpNightMode(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        if (mModalDialogManager != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mModalDialogManager.dismissAllDialogs(
                                    DialogDismissalCause.ACTIVITY_DESTROYED));
        }
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testMicPermissionDialog(boolean nightModeEnabled) throws IOException {
        AppModalPresenter presenter =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AppModalPresenter appModalPresenter = new AppModalPresenter(mActivity);
                            mModalDialogManager =
                                    new ModalDialogManager(appModalPresenter, ModalDialogType.APP);
                            new GlicMicPermissionDialogCoordinator(mActivity, mModalDialogManager)
                                    .show(result -> {});
                            return appModalPresenter;
                        });

        // Wait until the dialog is actually shown and laid out before rendering it.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true));
                    View view = presenter.getDialogViewForTesting();
                    Criteria.checkThat(view, Matchers.notNullValue());
                    Criteria.checkThat(view.getWidth(), Matchers.greaterThan(0));
                });

        View dialogView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return (View) presenter.getDialogViewForTesting();
                        });
        mRenderTestRule.render(dialogView, "glic_mic_permission_dialog");
    }
}
