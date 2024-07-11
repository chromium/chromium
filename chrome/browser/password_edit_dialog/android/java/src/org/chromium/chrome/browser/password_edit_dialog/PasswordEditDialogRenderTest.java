// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.password_edit_dialog.R.style.Theme_BrowserUI_DayNight;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the password edit dialog content view and compare them to a
 * gold standard.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordEditDialogRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("Default"),
                    new ParameterSet().value(true).name("NightMode"));

    private static final String USERNAME = "John Doe";
    private static final String PASSWORD = "passwordForTest";

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private PasswordEditDialogView mDialogView;

    public PasswordEditDialogRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(Theme_BrowserUI_DayNight);
        runOnUiThreadBlocking(
                () -> {
                    mDialogView =
                            (PasswordEditDialogView)
                                    mActivityTestRule
                                            .getActivity()
                                            .getLayoutInflater()
                                            .inflate(
                                                    R.layout.password_edit_dialog,
                                                    null);
                    mActivityTestRule.getActivity().setContentView(mDialogView);
                    ChromeRenderTestRule.sanitize(mDialogView);
                });
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testLayoutIsCorrect() throws IOException {
        setupViewWithModel();
        mRenderTestRule.render(mDialogView, "password_edit_dialog");
    }

    private void setupViewWithModel() {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                                    .with(
                                            PasswordEditDialogProperties.USERNAMES,
                                            Arrays.asList(new String[] {USERNAME}))
                                    .with(PasswordEditDialogProperties.USERNAME, USERNAME)
                                    .with(PasswordEditDialogProperties.PASSWORD, PASSWORD)
                                    .with(
                                            PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK,
                                            (String u) -> {})
                                    .with(
                                            PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK,
                                            (String u) -> {})
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                });
    }
}
