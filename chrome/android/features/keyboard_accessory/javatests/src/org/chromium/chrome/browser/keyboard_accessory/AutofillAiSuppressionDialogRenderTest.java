// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.view.View;

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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogParams;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for the Autofill AI suppression confirmation dialog. */
@DoNotBatch(reason = "The tests can't be batched because they run for different UI set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAiSuppressionDialogRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    private final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .setRevision(1)
                    .setDescription("Ligatures and contextual alternates disabled in UI text")
                    .build();

    private Activity mActivity;
    private ModalDialogManager mModalDialogManager;

    public AutofillAiSuppressionDialogRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mModalDialogManager = ((BlankUiTestActivity) mActivity).getModalDialogManager();
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        setRtlForTesting(false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAutofillAiSuppressionDialog() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    String rawBody =
                            "Suggested by Gemini · <src_link>View sources</src_link>\n"
                                + "You can remove this suggestion from Chrome. Your original source"
                                + " won't be deleted. <manage_link>Manage enhanced"
                                + " autofill</manage_link>";
                    CharSequence formattedBody =
                            SpanApplier.applySpans(
                                    rawBody,
                                    new SpanApplier.SpanInfo(
                                            "<src_link>",
                                            "</src_link>",
                                            new ChromeClickableSpan(mActivity, view -> {})),
                                    new SpanApplier.SpanInfo(
                                            "<manage_link>",
                                            "</manage_link>",
                                            new ChromeClickableSpan(mActivity, view -> {})));

                    ConfirmationDialogParams params =
                            new ConfirmationDialogParams.Builder(mActivity)
                                    .withTitle("Vehicle · AN-147338")
                                    .withDescription(formattedBody)
                                    .withSupportStopShowing(false)
                                    .withPositiveButton("Got it")
                                    .withNegativeButton("Remove from Chrome")
                                    .build();

                    ActionConfirmationDialog dialog =
                            new ActionConfirmationDialog(mActivity, mModalDialogManager);
                    dialog.show(
                            params,
                            (handler, result, stopShowing) ->
                                    DialogDismissType.DISMISS_IMMEDIATELY);
                });

        CriteriaHelper.pollUiThread(() -> mModalDialogManager.isShowing());

        View dialogView =
                runOnUiThreadBlocking(
                        () ->
                                ((AppModalPresenter)
                                                mModalDialogManager.getCurrentPresenterForTest())
                                        .getDialogViewForTesting());

        CriteriaHelper.pollUiThread(dialogView::isLaidOut);
        mRenderTestRule.render(dialogView, "autofill_ai_suppression_dialog");
    }
}
