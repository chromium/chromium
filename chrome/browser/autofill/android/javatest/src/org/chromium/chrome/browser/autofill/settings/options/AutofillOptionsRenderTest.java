// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.options;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.AfterClass;
import org.junit.Before;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for {@link AutofillOptionsFragment} in different states. */
@DoNotBatch(reason = "Render tests cannot be batched.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillOptionsRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static Bundle createFragmentArgs() {
        return AutofillOptionsFragment.createRequiredArgs(AutofillOptionsReferrer.SETTINGS);
    }

    @Rule
    public SettingsActivityTestRule<AutofillOptionsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillOptionsFragment.class, createFragmentArgs());

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_AUTOFILL)
                    .build();

    private EntityDataManager mEntityDataManager;

    public AutofillOptionsRenderTest(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mEntityDataManager = mock(EntityDataManager.class);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);
        when(mEntityDataManager.isPersonalContextPreferenceVisible()).thenReturn(true);
        when(mEntityDataManager.isPersonalContextEnabled()).thenReturn(true);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
    }

    @AfterClass
    public static void tearDownClass() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
        ChromeFeatureList.AUTOFILL_AI_REAUTH_REQUIRED
    })
    @DisableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testRenderAutofillOptionsSettings_WithPersonalContext() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        AutofillOptionsFragment fragment = mSettingsActivityTestRule.getFragment();

        // Wait for RecyclerView to perform a layout pass and attach child views.
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = fragment.getListView();
                    Criteria.checkThat(recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(recyclerView.getChildCount(), Matchers.greaterThan(1));
                });

        View view = fragment.getView();

        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "autofill_options_settings_with_personal_context");
    }
}
