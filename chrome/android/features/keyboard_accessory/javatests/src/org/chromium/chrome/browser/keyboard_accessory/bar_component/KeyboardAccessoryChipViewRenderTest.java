// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ActionBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DismissBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.AutofillProfilePayload;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.FillingProduct;
import org.chromium.components.autofill.FillingProductBridgeJni;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;

import java.util.ArrayList;
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
@EnableFeatures({
    ChromeFeatureList.ANDROID_ELEGANT_TEXT_HEIGHT,
    ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HOME_AND_WORK,
    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN
})
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
                    .setRevision(7)
                    .build();

    @Mock private KeyboardAccessoryView mKeyboardAccessoryView;
    @Mock private AutofillImageFetcher mMockImageFetcher;
    @Mock private FillingProductBridgeJni mMockFillingProductBridgeJni;
    @Mock private Profile mMockProfile;
    @Mock private PersonalDataManager mMockPersonalDataManager;

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
        FillingProductBridgeJni.setInstanceForTesting(mMockFillingProductBridgeJni);
        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.ADDRESS_ENTRY))
                .thenReturn(FillingProduct.ADDRESS);
        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.LOYALTY_CARD_ENTRY))
                .thenReturn(FillingProduct.LOYALTY_CARD);
        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
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
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN})
    public void renderSuggestions() throws Exception {
        // All suggestion types are rendered in the same test to minimize the number of render
        // tests.
        runOnUiThreadBlocking(
                () -> {
                    layoutViews();
                });
        mRenderTestRule.render(mContentView, "keyboard_accessory_suggestions");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderTwoLineSuggestions() throws Exception {
        // All suggestion types are rendered in the same test to minimize the number of render
        // tests.
        runOnUiThreadBlocking(
                () -> {
                    layoutViews();
                });
        mRenderTestRule.render(mContentView, "keyboard_accessory_two_line_suggestions");
    }

    private List<AutofillSuggestion> createSuggestionsToRender() {
        AutofillSuggestion addressSuggestion =
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

        AutofillProfile profile =
                AutofillProfile.builder().setRecordType(RecordType.ACCOUNT_HOME).build();
        when(mMockPersonalDataManager.getProfile("123")).thenReturn(profile);
        AutofillSuggestion homeAndWorkSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Carl Johnson")
                        .setSubLabel("carl@gmail.com")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph("")
                        .setApplyDeactivatedStyle(false)
                        .setPayload(new AutofillProfilePayload("123"))
                        .setIconId(R.drawable.home_logo)
                        .build();

        AutofillSuggestion autocompleteSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Autocomplete text")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .build();

        AutofillSuggestion creditCardSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Homer Simpson")
                        .setSubLabel("** 1234")
                        .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                        .setIconId(R.drawable.mc_card)
                        .build();

        AutofillSuggestion offerSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Offer suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                        .setIconId(R.drawable.ic_offer_tag)
                        .build();

        AutofillSuggestion otpSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Otp code")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ONE_TIME_PASSWORD_ENTRY)
                        .setIconId(R.drawable.ic_android_messages_icon)
                        .build();

        AutofillSuggestion passwordHistorySuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("email.address@gmail.com")
                        .setSubLabel("Recover password *********")
                        .setSuggestionType(SuggestionType.BACKUP_PASSWORD_ENTRY)
                        .setIconId(R.drawable.ic_history_24dp)
                        .build();

        return List.of(
                addressSuggestion,
                loyaltyCardSuggestion,
                homeAndWorkSuggestion,
                autocompleteSuggestion,
                creditCardSuggestion,
                offerSuggestion,
                otpSuggestion,
                passwordHistorySuggestion);
    }

    private ChipView createChipViewFromSuggestion(AutofillSuggestion suggestion) {
        Action action = new Action(AUTOFILL_SUGGESTION, unused -> {});
        BarItemViewHolder<AutofillBarItem, ChipView> viewHolder =
                KeyboardAccessoryViewBinder.create(
                        mKeyboardAccessoryView,
                        mUiConfiguration,
                        mContentView,
                        AutofillBarItem.getBarItemType(suggestion, mMockProfile));
        ChipView chipView = (ChipView) viewHolder.itemView;
        viewHolder.bind(new AutofillBarItem(suggestion, action, mMockProfile), chipView);
        chipView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return chipView;
    }

    private ChipView createCredmanEntry() {
        Action credmanAction = new Action(CREDMAN_CONDITIONAL_UI_REENTRY, unused -> {});
        BarItemViewHolder<BarItem, ChipView> viewHolder =
                KeyboardAccessoryViewBinder.create(
                        mKeyboardAccessoryView,
                        mUiConfiguration,
                        mContentView,
                        BarItem.Type.ACTION_CHIP);
        ChipView chipView = (ChipView) viewHolder.itemView;
        viewHolder.bind(
                new ActionBarItem(
                        BarItem.Type.ACTION_CHIP,
                        credmanAction,
                        org.chromium.chrome.browser.keyboard_accessory.R.string.select_passkey),
                chipView);
        chipView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return chipView;
    }

    private View createGeneratePassword() {
        Action generatePasswordAction = new Action(GENERATE_PASSWORD_AUTOMATIC, unused -> {});
        // TODO: crbug.com/385172647 - Use generics parameters once 2 line chips are rolled out.
        BarItemViewHolder viewHolder =
                KeyboardAccessoryViewBinder.create(
                        mKeyboardAccessoryView,
                        mUiConfiguration,
                        mContentView,
                        BarItem.Type.ACTION_BUTTON);
        View view = viewHolder.itemView;
        viewHolder.bind(
                new ActionBarItem(
                        BarItem.Type.ACTION_BUTTON,
                        generatePasswordAction,
                        org.chromium.chrome.browser.keyboard_accessory.R.string
                                .password_generation_accessory_button),
                view);
        view.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return view;
    }

    private View createDismissButton() {
        // TODO: crbug.com/385172647 - Use generics parameters once 2 line chips are rolled out.
        BarItemViewHolder viewHolder =
                KeyboardAccessoryViewBinder.create(
                        mKeyboardAccessoryView,
                        mUiConfiguration,
                        mContentView,
                        BarItem.Type.DISMISS_CHIP);
        View view = viewHolder.itemView;
        viewHolder.bind(new DismissBarItem(() -> {}), view);
        view.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return view;
    }

    private List<View> createKeyboardAccessoryItemsToRender() {
        List<View> items = new ArrayList<>();
        for (AutofillSuggestion suggestion : createSuggestionsToRender()) {
            items.add(createChipViewFromSuggestion(suggestion));
        }
        items.add(createCredmanEntry());
        items.add(createGeneratePassword());
        items.add(createDismissButton());
        return items;
    }

    private void layoutViews() {
        for (View view : createKeyboardAccessoryItemsToRender()) {
            mContentView.addView(view);
        }
    }
}
