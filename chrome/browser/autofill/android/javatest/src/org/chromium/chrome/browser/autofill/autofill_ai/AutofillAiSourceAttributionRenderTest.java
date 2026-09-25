// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.HeaderProperties;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.ItemType;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.SourceCardProperties;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.List;

/** Render tests for the Autofill AI source attribution bottom sheet. */
@DoNotBatch(reason = "The tests can't be batched because they run for different UI set-ups.")
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAiSourceAttributionRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            List.of(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .setRevision(1)
                    .build();

    private Activity mActivity;

    @BeforeClass
    public static void setUpBeforeClass() {
        BlankUiTestActivity.setTestTheme(R.style.Theme_BrowserUI_DayNight);
    }

    public AutofillAiSourceAttributionRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        BlankUiTestActivity.setTestTheme(0);
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        setRtlForTesting(false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAutofillAiSourceAttributionSheet() throws IOException {
        AutofillAiSourceAttributionView view =
                setUpView(
                        "Vehicle · AN-147338",
                        createCardModel(
                                R.drawable.ic_outline_email_24dp,
                                "Flight to San Francisco",
                                "Open Gmail source: Flight to San Francisco"),
                        createCardModel(
                                R.drawable.ic_photo_library_fill_24dp,
                                "Photos · album",
                                "Open Photos source: Photos · album"));
        ChromeRenderTestRule.sanitize(view.getContentView());
        mRenderTestRule.render(view.getContentView(), "autofill_ai_source_attribution_sheet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderSheet_LongDomainTruncation() throws IOException {
        AutofillAiSourceAttributionView view =
                setUpView(
                        "Vehicle · AN-147338",
                        createCardModel(
                                R.drawable.ic_outline_email_24dp,
                                "Very Long Application Name That Should Be"
                                        + " Truncated ·"
                                        + " subdomain.very-long-domain-name-"
                                        + "that-should-be-truncated-"
                                        + "-with-ellipsis.example.com",
                                "Open source description"));
        ChromeRenderTestRule.sanitize(view.getContentView());
        mRenderTestRule.render(
                view.getContentView(), "autofill_ai_source_attribution_sheet_long_domain");
    }

    private AutofillAiSourceAttributionView setUpView(String subtitle, PropertyModel... cards) {
        AutofillAiSourceAttributionView view =
                runOnUiThreadBlocking(
                        () -> {
                            var v = new AutofillAiSourceAttributionView(mActivity);
                            SimpleRecyclerViewAdapter adapter =
                                    new SimpleRecyclerViewAdapter(createModelList(subtitle, cards));
                            adapter.registerType(
                                    ItemType.HEADER,
                                    new LayoutViewBuilder<View>(
                                            R.layout.autofill_ai_attribution_header_item),
                                    AutofillAiSourceAttributionViewBinder::bindHeader);
                            adapter.registerType(
                                    ItemType.SOURCE_CARD,
                                    new LayoutViewBuilder<View>(
                                            R.layout.autofill_ai_source_card_item),
                                    AutofillAiSourceAttributionViewBinder::bindSourceCard);
                            v.setAdapter(adapter);
                            mActivity.setContentView(
                                    v.getContentView(),
                                    new FrameLayout.LayoutParams(
                                            ViewGroup.LayoutParams.MATCH_PARENT,
                                            ViewGroup.LayoutParams.WRAP_CONTENT));
                            return v;
                        });
        CriteriaHelper.pollUiThread(view.getContentView()::isLaidOut);
        return view;
    }

    private static PropertyModel createCardModel(
            int iconResId, String title, String contentDescription) {
        return runOnUiThreadBlocking(
                () ->
                        new PropertyModel.Builder(SourceCardProperties.ALL_KEYS)
                                .with(SourceCardProperties.ICON_RES_ID, iconResId)
                                .with(SourceCardProperties.TITLE, title)
                                .with(SourceCardProperties.CONTENT_DESCRIPTION, contentDescription)
                                .build());
    }

    private static ModelList createModelList(String subtitle, PropertyModel... cards) {
        ModelList modelList = new ModelList();
        PropertyModel headerModel =
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.SUBTITLE, subtitle)
                        .build();
        modelList.add(new ListItem(ItemType.HEADER, headerModel));
        for (PropertyModel card : cards) {
            modelList.add(new ListItem(ItemType.SOURCE_CARD, card));
        }
        return modelList;
    }
}
