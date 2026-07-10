// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.personal_context;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.AfterClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for {@link AutofillPersonalContextFragment} in different states. */
@DoNotBatch(reason = "Render tests cannot be batched.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillPersonalContextSettingsRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public SettingsActivityTestRule<AutofillPersonalContextFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillPersonalContextFragment.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_AUTOFILL)
                    .build();

    public AutofillPersonalContextSettingsRenderTest(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownClass() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderPersonalContextSettings_Collapsed() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        AutofillPersonalContextFragment fragment = mSettingsActivityTestRule.getFragment();

        // Wait for RecyclerView to perform a layout pass and attach child views.
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = fragment.getListView();
                    Criteria.checkThat(recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(recyclerView.getChildCount(), Matchers.greaterThan(1));
                });

        View view = fragment.getView();

        // Render Collapsed UI (default state upon entering the screen).
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "autofill_personal_context_collapsed");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderPersonalContextSettings_Expanded() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        Activity activity = mSettingsActivityTestRule.getActivity();
        View expandedView =
                runOnUiThreadBlocking(
                        () -> {
                            View view =
                                    LayoutInflater.from(activity)
                                            .inflate(
                                                    R.layout
                                                            .autofill_personal_context_expandable_notice,
                                                    null);
                            view.setBackgroundColor(
                                    SemanticColorUtils.getSettingsBackgroundColor(activity));
                            ((ViewGroup) activity.findViewById(android.R.id.content)).addView(view);
                            return view;
                        });

        // Wait for view to be laid out.
        CriteriaHelper.pollUiThread(expandedView::isLaidOut);

        // Render Expanded UI by inflating the notices layout directly.
        ChromeRenderTestRule.sanitize(expandedView);
        mRenderTestRule.render(expandedView, "autofill_personal_context_expanded");
    }
}
