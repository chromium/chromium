// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the keyboard accessory bar suggestions and compare them to a
 * gold standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ANDROID_ELEGANT_TEXT_HEIGHT)
public class KeyboardAccessoryChipViewRenderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Mock private KeyboardAccessoryView mKeyboardAccessoryView;
    @Mock private AutofillImageFetcher mMockImageFetcher;

    private ViewGroup mContentView;
    private KeyboardAccessoryViewBinder.UiConfiguration mUiConfiguration;

    public KeyboardAccessoryChipViewRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView =
                runOnUiThreadBlocking(
                        () -> {
                            LinearLayout contentView = new LinearLayout(activity);
                            contentView.setOrientation(LinearLayout.VERTICAL);
                            contentView.setBackgroundColor(Color.WHITE);

                            activity.setContentView(
                                    contentView,
                                    new LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                            return contentView;
                        });
        mUiConfiguration =
                KeyboardAccessoryCoordinator.createUiConfiguration(activity, mMockImageFetcher);
    }

    @After
    public void tearDown() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
        setRtlForTesting(false);
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderSuggestions() throws Exception {
        AutofillSuggestion addessSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Homer Simpson")
                        .setSubLabel("hsimpson@gmail.com")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph("")
                        .setApplyDeactivatedStyle(false)
                        .build();
        AutofillSuggestion loyaltyCardSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("987654321")
                        .setSubLabel("Deutsche Bahn GmbH")
                        .setSuggestionType(SuggestionType.LOYALTY_CARD_ENTRY)
                        .setFeatureForIph("")
                        .setApplyDeactivatedStyle(false)
                        .setCustomIconUrl(new GURL(""))
                        .build();

        // All suggestion types are rendered in the same test to minimize the number of render
        // tests.
        runOnUiThreadBlocking(() -> createChipViewFromSuggestion(addessSuggestion));
        runOnUiThreadBlocking(() -> createChipViewFromSuggestion(loyaltyCardSuggestion));
        mRenderTestRule.render(mContentView, "keyboard_accessory_suggestions");
    }

    private void createChipViewFromSuggestion(AutofillSuggestion suggestion) {
        KeyboardAccessoryData.Action action =
                new KeyboardAccessoryData.Action(AUTOFILL_SUGGESTION, unused -> {});
        BarItemViewHolder<AutofillBarItem, ChipView> viewHolder =
                KeyboardAccessoryViewBinder.create(
                        mKeyboardAccessoryView,
                        mUiConfiguration,
                        mContentView,
                        BarItem.Type.SUGGESTION);
        ChipView chipView = (ChipView) viewHolder.itemView;
        viewHolder.bind(new AutofillBarItem(suggestion, action), chipView);
        chipView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mContentView.addView(chipView);
    }
}
