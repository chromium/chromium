// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.fre;

import android.app.Activity;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.signin.ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;

/**
 * Render tests for signin FRE UMA dialog.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class FreUMADialogRenderTest {
    @ClassRule
    public static BaseActivityTestRule<DummyUiActivity> activityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    private static Activity sActivity;

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private FreUMADialogCoordinator mCoordinator;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sActivity = activityTestRule.getActivity(); });
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new FreUMADialogCoordinator(sActivity,
                    new ModalDialogManager(new AppModalPresenter(sActivity), ModalDialogType.APP));
        });
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(mCoordinator::dismissDialogForTesting);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFreUMADialogView(boolean nightModeEnabled) throws IOException {
        CriteriaHelper.pollUiThread(() -> {
            return mCoordinator.getDialogViewForTesting()
                    .findViewById(R.id.fre_uma_dialog_dismiss_button)
                    .isShown();
        });
        mRenderTestRule.render(mCoordinator.getDialogViewForTesting(), "fre_uma_dialog");
    }
}
