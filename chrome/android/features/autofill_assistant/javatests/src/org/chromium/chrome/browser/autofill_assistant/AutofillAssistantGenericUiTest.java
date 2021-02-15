// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_YES;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.PositionAssertions.isLeftAlignedWith;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.PickerActions.setDate;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.isIn;
import static org.hamcrest.Matchers.iterableWithSize;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTypefaceSpan;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isImportantForAccessibility;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.withMinimumSize;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.withTextGravity;

import android.graphics.Typeface;
import android.support.test.InstrumentationRegistry;
import android.view.Gravity;
import android.widget.CheckBox;
import android.widget.DatePicker;
import android.widget.RadioButton;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantDimension;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.AutofillFormatProto;
import org.chromium.chrome.browser.autofill_assistant.proto.BooleanAndProto;
import org.chromium.chrome.browser.autofill_assistant.proto.BooleanList;
import org.chromium.chrome.browser.autofill_assistant.proto.BooleanNotProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CallbackProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClearViewContainerProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientDimensionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataResultProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ColorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ComputeValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CreateCreditCardResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CreateNestedGenericUiProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CreditCardResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DateFormatProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DateList;
import org.chromium.chrome.browser.autofill_assistant.proto.DateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DividerViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.EndActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.EventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ForEachProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GenericUserInterfaceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ImageViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.IntList;
import org.chromium.chrome.browser.autofill_assistant.proto.IntegerSumProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InteractionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InteractionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.LinearLayoutProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ModelProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnModelValueChangedEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnTextLinkClickedProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnUserActionCalled;
import org.chromium.chrome.browser.autofill_assistant.proto.OnViewClickedEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetModelValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetUserActionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetViewEnabledProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetViewVisibilityProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShapeDrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCalendarPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiProto.PeriodicElementChecks;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiProto.PeriodicElementChecks.ElementCheck;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowInfoPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowListPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StringList;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ToStringProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ToggleButtonViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ToggleUserActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserActionList;
import org.chromium.chrome.browser.autofill_assistant.proto.UserActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueComparisonProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.VerticalExpanderAccordionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.VerticalExpanderViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewAttributesProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewContainerProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewLayoutParamsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant generic UI framework.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantGenericUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        mTestRule.getTestServer().getURL(TEST_PAGE),
                        /* startImmediately = */ true));
        mTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getScrimCoordinator()
                .disableAnimationForTesting(true);
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
    }

    private ViewProto createTestImage(String resourceId, String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setImageView(ImageViewProto.newBuilder().setImage(
                        DrawableProto.newBuilder().setResourceIdentifier(resourceId)))
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setLayoutWidth(24)
                                .setLayoutHeight(24)
                                .setLayoutGravity(ViewLayoutParamsProto.Gravity.CENTER.getNumber()))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createTextView(String text, String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setTextView(TextViewProto.newBuilder().setText(text).setTextAppearance(
                        "TextAppearance.TextMedium.Secondary"))
                .setAttributes(ViewAttributesProto.newBuilder().setPaddingStart(24))
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setLayoutWidth(0)
                                .setLayoutWeight(1.0f)
                                .setLayoutHeight(ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createRadioButtonView(ViewProto contentView, String radioGroup,
            String viewIdentifier, String modelIdentifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setToggleButtonView(
                        ToggleButtonViewProto.newBuilder()
                                .setRadioButton(ToggleButtonViewProto.RadioButton.newBuilder()
                                                        .setRadioGroupIdentifier(radioGroup))
                                .setRightContentView(contentView)
                                .setModelIdentifier(modelIdentifier))
                .setIdentifier(viewIdentifier)
                .build();
    }

    private ViewProto createCheckBoxView(
            String text, String viewIdentifier, String modelIdentifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setToggleButtonView(
                        ToggleButtonViewProto.newBuilder()
                                .setCheckBox(ToggleButtonViewProto.CheckBox.newBuilder())
                                .setRightContentView(ViewProto.newBuilder().setTextView(
                                        TextViewProto.newBuilder().setText(text)))
                                .setModelIdentifier(modelIdentifier))
                .setIdentifier(viewIdentifier)
                .build();
    }

    private ViewProto createSectionView(List<ViewProto> views, String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setViewContainer(
                        ViewContainerProto.newBuilder()
                                .setLinearLayout(LinearLayoutProto.newBuilder().setOrientation(
                                        LinearLayoutProto.Orientation.HORIZONTAL))
                                .addAllViews(views))
                .setAttributes(ViewAttributesProto.newBuilder()
                                       .setPaddingStart(24)
                                       .setPaddingTop(24)
                                       .setPaddingEnd(24)
                                       .setPaddingBottom(24))
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setLayoutWidth(ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE)
                                .setLayoutHeight(ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createSectionDividerView(String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setDividerView(DividerViewProto.newBuilder())
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setMarginStart(0)
                                .setMarginEnd(0)
                                .setLayoutWidth(ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE)
                                .setLayoutHeight(ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createSimpleTextView(String identifier, String text) {
        return (ViewProto) ViewProto.newBuilder()
                .setIdentifier(identifier)
                .setTextView(TextViewProto.newBuilder().setText(text))
                .build();
    }

    // A simple view that takes its text from the provided model identifier.
    private ViewProto createTextModelView(String identifier, String modelIdentifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setIdentifier(identifier)
                .setTextView(TextViewProto.newBuilder().setModelIdentifier(modelIdentifier))
                .build();
    }

    private CallbackProto createAutofillToStringCallback(
            String inputModelIdentifier, String resultModelIdentifier, String autofillFormat) {
        return (CallbackProto) CallbackProto.newBuilder()
                .setComputeValue(
                        ComputeValueProto.newBuilder()
                                .setResultModelIdentifier(resultModelIdentifier)
                                .setToString(
                                        ToStringProto.newBuilder()
                                                .setValue(ValueReferenceProto.newBuilder()
                                                                  .setModelIdentifier(
                                                                          inputModelIdentifier))
                                                .setAutofillFormat(
                                                        AutofillFormatProto.newBuilder().setPattern(
                                                                autofillFormat))))
                .build();
    }

    private ValueReferenceProto createValueReference(String modelIdentifier) {
        return (ValueReferenceProto) ValueReferenceProto.newBuilder()
                .setModelIdentifier(modelIdentifier)
                .build();
    }

    private ValueReferenceProto createEmptyValue() {
        return (ValueReferenceProto) ValueReferenceProto.newBuilder()
                .setValue(ValueProto.getDefaultInstance())
                .build();
    }

    private ValueComparisonProto createValueComparison(ValueReferenceProto valueA,
            ValueReferenceProto valueB, ValueComparisonProto.Mode mode) {
        return (ValueComparisonProto) ValueComparisonProto.newBuilder()
                .setMode(mode)
                .setValueA(valueA)
                .setValueB(valueB)
                .build();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1033877")
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testStaticUserInterface() {
        DrawableProto roundedRect =
                (DrawableProto) DrawableProto.newBuilder()
                        .setShape(
                                ShapeDrawableProto.newBuilder()
                                        .setRectangle(ShapeDrawableProto.Rectangle.newBuilder()
                                                              .setCornerRadius(ClientDimensionProto
                                                                                       .newBuilder()
                                                                                       .setDp(16)))
                                        .setStrokeColor(ColorProto.newBuilder().setParseableColor(
                                                "#DADCE0"))
                                        .setStrokeWidth(ClientDimensionProto.newBuilder().setDp(1)))
                        .build();

        ViewProto locationImage = createTestImage("btn_close", "locationImage");
        ViewProto locationTextView =
                createTextView("345 Spear Street, San Francisco", "locationText");
        ViewProto locationChevron = createTestImage("ic_expand_more_black_24dp", "locationChevron");
        ViewProto locationSection = createSectionView(
                Arrays.asList(locationImage, locationTextView, locationChevron), "locationSection");

        ViewProto cardImage = createTestImage("btn_close", "cardImage");
        ViewProto cardTextView = createTextView("Visa •••• 1111", "cardText");
        ViewProto cardChevron = createTestImage("ic_expand_more_black_24dp", "cardChevron");
        ViewProto cardSection = createSectionView(
                Arrays.asList(cardImage, cardTextView, cardChevron), "cardSection");

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setAttributes(ViewAttributesProto.newBuilder().setBackground(roundedRect))
                        .setLayoutParams(
                                ViewLayoutParamsProto.newBuilder()
                                        .setMarginStart(24)
                                        .setMarginEnd(24)
                                        .setLayoutWidth(
                                                ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE)
                                        .setLayoutHeight(
                                                ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(locationSection)
                                        .addViews(createSectionDividerView("divider"))
                                        .addViews(cardSection))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 GenericUserInterfaceProto.newBuilder().setRootView(
                                                         rootView))
                                         .setRequestTermsAndConditions(false))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("End").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("345 Spear Street, San Francisco")).check(matches(isDisplayed()));
        onView(withText("Visa •••• 1111")).check(matches(isDisplayed()));

        onView(withText("345 Spear Street, San Francisco"))
                .check(isLeftAlignedWith(withText("Visa •••• 1111")));
        onView(withTagValue(is("locationChevron")))
                .check(isLeftAlignedWith(withTagValue(is("cardChevron"))));

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("End"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    @DisabledTest(message = "crbug.com/1087042")
    public void testOnViewClickedWriteToModel() {
        ViewProto clickableView1 = (ViewProto) ViewProto.newBuilder()
                                           .setTextView(TextViewProto.newBuilder().setText(
                                                   "Writes 'true' to output_1 when clicked"))
                                           .setIdentifier("clickableView1")
                                           .build();
        ViewProto clickableView2 = (ViewProto) ViewProto.newBuilder()
                                           .setTextView(TextViewProto.newBuilder().setText(
                                                   "Writes 'Hello World' to output_2 when clicked"))
                                           .setIdentifier("clickableView2")
                                           .build();

        ViewProto rootViewPrepended =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView1))
                        .build();
        ViewProto rootViewAppended =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView2))
                        .build();

        List<InteractionProto> interactionsPrepended = new ArrayList<>();
        interactionsPrepended.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickableView1")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output_1")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setBooleans(
                                                        BooleanList.newBuilder().addValues(
                                                                true))))))
                        .build());
        List<InteractionProto> interactionsAppended = new ArrayList<>();
        interactionsAppended.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickableView2")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output_2")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addValues(
                                                                "Hello World"))))))
                        .build());

        List<ModelProto.ModelValue> modelValuesPrepended = new ArrayList<>();
        modelValuesPrepended.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                         .setIdentifier("output_1")
                                         .build());
        List<ModelProto.ModelValue> modelValuesAppended = new ArrayList<>();
        modelValuesAppended.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                        .setIdentifier("output_2")
                                        .build());

        GenericUserInterfaceProto genericUserInterfacePrepended =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(rootViewPrepended)
                        .setInteractions(InteractionsProto.newBuilder().addAllInteractions(
                                interactionsPrepended))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValuesPrepended))
                        .build();

        GenericUserInterfaceProto genericUserInterfaceAppended =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(rootViewAppended)
                        .setInteractions(InteractionsProto.newBuilder().addAllInteractions(
                                interactionsAppended))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValuesAppended))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 genericUserInterfacePrepended)
                                         .setGenericUserInterfaceAppended(
                                                 genericUserInterfaceAppended)
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("Writes 'true' to output_1 when clicked")).perform(click());
        onView(withText("Writes 'Hello World' to output_2 when clicked")).perform(click());

        // Finish action, wait for response and prepare next set of actions.
        List<ActionProto> nextActions = new ArrayList<>();
        nextActions.add(
                (ActionProto) ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder()
                                           .setMessage("Finished")
                                           .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                   ChipProto.newBuilder()
                                                           .setType(ChipType.DONE_ACTION)
                                                           .setText("End"))))
                        .build());
        testService.setNextActions(nextActions);
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(2));
        assertThat(resultModelValues,
                containsInAnyOrder((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                           .setIdentifier("output_1")
                                           .setValue(ValueProto.newBuilder().setBooleans(
                                                   BooleanList.newBuilder().addValues(true)))
                                           .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_2")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build()));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testCallbackChain() {
        ViewProto clickableView =
                (ViewProto) ViewProto.newBuilder()
                        .setTextView(TextViewProto.newBuilder().setText(
                                "Writes 'Hello World' to output_1 and output_3 when clicked"))
                        .setIdentifier("clickableView")
                        .build();

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickableView")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output_1")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addValues(
                                                                "Hello World"))))))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output_3")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addValues(
                                                                "Hello World"))))))
                        .build());
        // Whenever output_1 changes, copy the value to output_2.
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "output_1")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output_2")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addValues(
                                                                "Hello World"))))))
                        .build());
        // Whenever output_2 changes, copy the value to output_1. This tests that no infinite loop
        // is created, because events should only be fired for actual value changes.
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "output_2")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output_1")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addValues(
                                                                "Hello World"))))))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_1")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_2")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_3")
                                .build());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 GenericUserInterfaceProto.newBuilder()
                                                         .setRootView(rootView)
                                                         .setInteractions(
                                                                 InteractionsProto.newBuilder()
                                                                         .addAllInteractions(
                                                                                 interactions))
                                                         .setModel(ModelProto.newBuilder()
                                                                           .addAllValues(
                                                                                   modelValues)))
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("Writes 'Hello World' to output_1 and output_3 when clicked"))
                .perform(click());

        // Finish action, wait for response and prepare next set of actions.
        List<ActionProto> nextActions = new ArrayList<>();
        nextActions.add(
                (ActionProto) ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder()
                                           .setMessage("Finished")
                                           .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                   ChipProto.newBuilder()
                                                           .setType(ChipType.DONE_ACTION)
                                                           .setText("End"))))
                        .build());
        testService.setNextActions(nextActions);
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(3));
        assertThat(resultModelValues,
                containsInAnyOrder(
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_1")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_2")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_3")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build()));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testShowInfoPopupOnClick() {
        ViewProto clickableView = (ViewProto) ViewProto.newBuilder()
                                          .setTextView(TextViewProto.newBuilder().setText(
                                                  "Shows an info popup when clicked"))
                                          .setIdentifier("clickableView")
                                          .build();

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                         OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                                 "clickableView")))
                                 .addCallbacks(CallbackProto.newBuilder().setShowInfoPopup(
                                         ShowInfoPopupProto.newBuilder().setInfoPopup(
                                                 InfoPopupProto.newBuilder()
                                                         .setText("Info message here")
                                                         .setTitle("Some title"))))
                                 .build());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 GenericUserInterfaceProto.newBuilder()
                                                         .setRootView(rootView)
                                                         .setInteractions(
                                                                 InteractionsProto.newBuilder()
                                                                         .addAllInteractions(
                                                                                 interactions)))
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("Shows an info popup when clicked")).perform(click());
        onView(withText("Some title")).check(matches(isDisplayed()));
        onView(withText("Info message here")).check(matches(isDisplayed()));
        onView(withText(mTestRule.getActivity().getString(R.string.close))).perform(click());
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testListPopup() {
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickableView")))
                        .addCallbacks(CallbackProto.newBuilder().setShowListPopup(
                                ShowListPopupProto.newBuilder()
                                        .setItemNames(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addAllValues(
                                                                Arrays.asList("08:00 AM",
                                                                        "08:30 AM", "09:00 AM",
                                                                        "09:30 AM", "10:00 AM")))))
                                        .setSelectedItemIndicesModelIdentifier(
                                                "selected_items_indices")
                                        .setSelectedItemNamesModelIdentifier("selected_item_names")
                                        .setAllowMultiselect(false)))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("selected_items_indices")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("selected_item_names")
                                .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder()
                                             .setTextView(TextViewProto.newBuilder().setText(
                                                     "Shows a list popup when clicked"))
                                             .setIdentifier("clickableView"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(
                                 ShowGenericUiProto.newBuilder()
                                         .setGenericUserInterface(genericUserInterface)
                                         .addAllOutputModelIdentifiers(Arrays.asList(
                                                 "selected_items_indices", "selected_item_names")))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());

        onView(withText("Shows a list popup when clicked")).perform(click());
        onView(withText("09:30 AM")).inRoot(isDialog()).perform(click());

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Done")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(2));
        assertThat((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                           .setIdentifier("selected_items_indices")
                           .setValue(ValueProto.newBuilder().setInts(
                                   IntList.newBuilder().addValues(3)))
                           .build(),
                isIn(resultModelValues));
        assertThat((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                           .setIdentifier("selected_item_names")
                           .setValue(ValueProto.newBuilder().setStrings(
                                   StringList.newBuilder().addValues("09:30 AM")))
                           .build(),
                isIn(resultModelValues));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testMandatoryFields() {
        ViewProto clickableView1 = (ViewProto) ViewProto.newBuilder()
                                           .setTextView(TextViewProto.newBuilder().setText(
                                                   "Writes 'true' to output1 when clicked"))
                                           .setIdentifier("clickableView1")
                                           .build();
        ViewProto clickableView2 = (ViewProto) ViewProto.newBuilder()
                                           .setTextView(TextViewProto.newBuilder().setText(
                                                   "Writes 'true' to output2 when clicked"))
                                           .setIdentifier("clickableView2")
                                           .build();

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView1)
                                        .addViews(clickableView2))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickableView1")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output1")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setBooleans(
                                                        BooleanList.newBuilder().addValues(
                                                                true))))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickableView2")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("output2")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setBooleans(
                                                        BooleanList.newBuilder().addValues(
                                                                true))))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "output1")))
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "output2")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setBooleanAnd(BooleanAndProto.newBuilder().addAllValues(
                                                Arrays.asList(ValueReferenceProto.newBuilder()
                                                                      .setModelIdentifier("output1")
                                                                      .build(),
                                                        ValueReferenceProto.newBuilder()
                                                                .setModelIdentifier("output2")
                                                                .build())))
                                        .setResultModelIdentifier("combined")))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("combined")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        GenericUserInterfaceProto genericUserInterfacePrepended =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(rootView)
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setGenericUserInterfacePrepended(
                                                            genericUserInterfacePrepended)
                                                    .setAdditionalModelIdentifierToCheck("combined")
                                                    .setRequestTermsAndConditions(false))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Finished")
                                            .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("End"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));

        onView(withText("Writes 'true' to output1 when clicked")).perform(click());
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));
        onView(withText("Writes 'true' to output2 when clicked")).perform(click());
        onView(withContentDescription("Continue")).check(matches(isEnabled()));

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("End"), isCompletelyDisplayed());
    }

    /**
     * Shows chips and tests the EndAction interaction. Only the subset of specified output values
     * should be returned to the backend.
     */
    @Test
    @MediumTest
    public void testGenericUiChipsEndAction() {
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("value_a")
                                .setValue(ValueProto.newBuilder().setInts(
                                        IntList.newBuilder().addValues(1)))
                                .build());
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addOutputModelIdentifiers("value_a"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Done")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        // Test that output values only contain |value_a|, but not |chips|.
        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(1));
        assertThat(resultModelValues.get(0),
                is((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("value_a")
                                .setValue(ValueProto.newBuilder().setInts(
                                        IntList.newBuilder().addValues(1)))
                                .build()));
    }

    /**
     * Displays a calendar popup and interacts with it.
     */
    @Test
    @MediumTest
    public void testCalendarPopup() {
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("date")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("date_string")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("date not set")))
                                .build());
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "text_view")))
                        .addCallbacks(CallbackProto.newBuilder().setShowCalendarPopup(
                                ShowCalendarPopupProto.newBuilder()
                                        .setDateModelIdentifier("date")
                                        .setMinDate(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setDates(
                                                        DateList.newBuilder().addValues(
                                                                DateProto.newBuilder()
                                                                        .setYear(2020)
                                                                        .setMonth(1)
                                                                        .setDay(1)))))
                                        .setMaxDate(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setDates(
                                                        DateList.newBuilder().addValues(
                                                                DateProto.newBuilder()
                                                                        .setYear(2020)
                                                                        .setMonth(12)
                                                                        .setDay(31)))))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "date")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("date_string")
                                        .setToString(
                                                ToStringProto.newBuilder()
                                                        .setValue(
                                                                ValueReferenceProto.newBuilder()
                                                                        .setModelIdentifier("date"))
                                                        .setDateFormat(
                                                                DateFormatProto.newBuilder()
                                                                        .setDateFormat(
                                                                                "EEE, MMM d y")))))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setIdentifier("text_view")
                                        .setLayoutParams(
                                                ViewLayoutParamsProto.newBuilder().setMinimumHeight(
                                                        48))
                                        .setTextView(TextViewProto.newBuilder().setModelIdentifier(
                                                "date_string")))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addOutputModelIdentifiers("date"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("date not set"), isCompletelyDisplayed());
        onView(withText("date not set"))
                .check(matches(withMinimumSize(0,
                        AssistantDimension.getPixelSizeDp(
                                InstrumentationRegistry.getContext(), 48))));

        onView(withText("date not set")).perform(click());
        onView(withClassName(equalTo(DatePicker.class.getName())))
                .inRoot(isDialog())
                .perform(setDate(2020, 6, 7));
        onView(withText(R.string.date_picker_dialog_set)).inRoot(isDialog()).perform(click());
        waitUntilViewMatchesCondition(withText("Sun, Jun 7, 2020"), isCompletelyDisplayed());

        onView(withText("Sun, Jun 7, 2020")).perform(click());
        onView(withClassName(equalTo(DatePicker.class.getName())))
                .inRoot(isDialog())
                .perform(setDate(2020, 7, 13));
        onView(withText(R.string.date_picker_dialog_set)).inRoot(isDialog()).perform(click());
        waitUntilViewMatchesCondition(withText("Mon, Jul 13, 2020"), isCompletelyDisplayed());

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Done")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(1));
        assertThat(resultModelValues.get(0),
                is((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("date")
                                .setValue(ValueProto.newBuilder().setDates(
                                        DateList.newBuilder().addValues(DateProto.newBuilder()
                                                                                .setYear(2020)
                                                                                .setMonth(7)
                                                                                .setDay(13))))
                                .build()));
    }

    /**
     * Tests custom content descriptions for views.
     */
    @Test
    @MediumTest
    public void testContentDescription() {
        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder().setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "auto-generated-desc")))
                                        .addViews(
                                                ViewProto.newBuilder()
                                                        .setTextView(
                                                                TextViewProto.newBuilder().setText(
                                                                        "no-desc"))
                                                        .setAttributes(
                                                                ViewAttributesProto.newBuilder()
                                                                        .setContentDescription("")))
                                        .addViews(
                                                ViewProto.newBuilder()
                                                        .setTextView(
                                                                TextViewProto.newBuilder().setText(
                                                                        "custom-desc"))
                                                        .setAttributes(
                                                                ViewAttributesProto.newBuilder()
                                                                        .setContentDescription(
                                                                                "custom")))))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("auto-generated-desc"), isCompletelyDisplayed());

        onView(withText("auto-generated-desc"))
                .check(matches(isImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES)));
        onView(withText("no-desc"))
                .check(matches(isImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO)));
        onView(withText("custom-desc")).check(matches(withContentDescription("custom")));
    }

    /**
     * Shows a chip that becomes enabled/disabled based on other interactions.
     */
    @Test
    @MediumTest
    public void testGenericUiDisableChips() {
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "enabled")))
                        .addCallbacks(CallbackProto.newBuilder().setToggleUserAction(
                                ToggleUserActionProto.newBuilder()
                                        .setUserActionsModelIdentifier("chips")
                                        .setEnabled(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "enabled"))
                                        .setUserActionIdentifier("done_chip")))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "text_view")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("enabled")
                                        .setBooleanNot(BooleanNotProto.newBuilder().setValue(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "enabled")))))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("enabled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(createTextView("Click me to toggle the chip", "text_view"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withContentDescription("Done")).check(matches(not(isEnabled())));

        onView(withText("Click me to toggle the chip")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("Done"), isEnabled());

        onView(withText("Click me to toggle the chip")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("Done"), not(isEnabled()));
    }

    /**
     * Shows a simple text view that, when clicked 3 or more times, will show a popup.
     */
    @Test
    @MediumTest
    public void testShowPopupIfClickedAtLeastThreeTimes() {
        List<InteractionProto> interactions = new ArrayList<>();
        ValueReferenceProto increment = ValueReferenceProto.newBuilder()
                                                .setValue(ValueProto.newBuilder().setInts(
                                                        IntList.newBuilder().addValues(1)))
                                                .build();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "text_view")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("counter")
                                        .setIntegerSum(IntegerSumProto.newBuilder().addAllValues(
                                                Arrays.asList(ValueReferenceProto.newBuilder()
                                                                      .setModelIdentifier("counter")
                                                                      .build(),
                                                        increment)))))
                        .addCallbacks(CallbackProto.newBuilder()
                                              .setConditionModelIdentifier("condition")
                                              .setShowInfoPopup(
                                                      ShowInfoPopupProto.newBuilder().setInfoPopup(
                                                              InfoPopupProto.newBuilder()
                                                                      .setTitle("Info popup title")
                                                                      .setText("Info text"))))
                        .build());
        ValueReferenceProto target = ValueReferenceProto.newBuilder()
                                             .setValue(ValueProto.newBuilder().setInts(
                                                     IntList.newBuilder().addValues(3)))
                                             .build();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "counter")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("condition")
                                        .setComparison(createValueComparison(
                                                createValueReference("counter"), target,
                                                ValueComparisonProto.Mode.GREATER_OR_EQUAL))))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("counter")
                                .setValue(ValueProto.newBuilder().setInts(
                                        IntList.newBuilder().addValues(0)))
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("condition")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(createTextView("Click me three+ times", "text_view"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Click me three+ times"), isCompletelyDisplayed());
        onView(withText("Click me three+ times")).perform(click());
        onView(withText("Info popup title")).check(doesNotExist());
        onView(withText("Click me three+ times")).perform(click());
        onView(withText("Info popup title")).check(doesNotExist());
        onView(withText("Click me three+ times")).perform(click());
        onView(withText("Info popup title")).check(matches(isDisplayed()));
    }

    /**
     * Shows two textviews. Clicking the first one will show/hide the other one.
     */
    @Test
    @MediumTest
    public void testViewVisibility() {
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "toggle_view")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("visible")
                                        .setBooleanNot(BooleanNotProto.newBuilder().setValue(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "visible")))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "visible")))
                        .addCallbacks(CallbackProto.newBuilder().setSetViewVisibility(
                                SetViewVisibilityProto.newBuilder()
                                        .setViewIdentifier("text_view")
                                        .setVisible(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "visible"))))
                        .build());

        // Set visibility initially to false for text_view.
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("visible")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder().setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(
                                                ViewProto.newBuilder()
                                                        .setIdentifier("toggle_view")
                                                        .setTextView(
                                                                TextViewProto.newBuilder().setText(
                                                                        "toggle view")))
                                        .addViews(
                                                ViewProto.newBuilder()
                                                        .setIdentifier("text_view")
                                                        .setTextView(
                                                                TextViewProto.newBuilder().setText(
                                                                        "text view")))))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("toggle view"), isCompletelyDisplayed());
        onView(withText("text view")).check(matches(not(isDisplayed())));

        onView(withText("toggle view")).perform(click());
        onView(withText("text view")).check(matches(isDisplayed()));

        onView(withText("toggle view")).perform(click());
        onView(withText("text view")).check(matches(not(isDisplayed())));
    }

    /**
     * Displays a text input widget and interacts with it.
     */
    @Test
    @MediumTest
    public void testTextInput() {
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("text_value")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("")))
                                .build());
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder()
                                             .setIdentifier("text_view")
                                             .setTextInputView(
                                                     TextInputViewProto.newBuilder()
                                                             .setHint("Type here")
                                                             .setType(TextInputViewProto
                                                                              .InputTypeHint.NONE)
                                                             .setModelIdentifier("text_value")))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addOutputModelIdentifiers("text_value"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withContentDescription("Type here"), isCompletelyDisplayed());
        onView(withContentDescription("Type here")).perform(replaceText("test 1"));
        onView(withText("test 1")).check(matches(isDisplayed()));
        onView(withContentDescription("Type here")).perform(replaceText("test 2"));
        onView(withText("test 2")).check(matches(isDisplayed()));

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Done")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(1));
        assertThat(resultModelValues.get(0),
                is((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("text_value")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("test 2")))
                                .build()));
    }

    /**
     * Tests text spans like <b></b> and <i></i> as well as text links like <link1></link1>.
     */
    @Test
    @MediumTest
    public void testTextSpansAndLinks() {
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("text_link_clicked")
                                .build());

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnTextLinkClicked(
                                OnTextLinkClickedProto.newBuilder().setTextLink(1)))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setBooleans(
                                                        BooleanList.newBuilder().addValues(true))))
                                        .setModelIdentifier("text_link_clicked")))
                        .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                EndActionProto.newBuilder().setStatus(
                                        ProcessedActionStatusProto.ACTION_APPLIED)))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder().setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "<b>bold text</b>")))
                                        .addViews(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "<i>italic text</i>")))
                                        .addViews(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "<link1>click here</link1>")))))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addOutputModelIdentifiers("text_link_clicked"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("click here"), isCompletelyDisplayed());

        onView(withText("bold text"))
                .check(matches(hasTypefaceSpan(0, "bold text".length() - 1, Typeface.BOLD)));
        onView(withText("italic text"))
                .check(matches(hasTypefaceSpan(0, "italic text".length() - 1, Typeface.ITALIC)));

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("click here")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(1));
        assertThat(resultModelValues.get(0),
                is((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("text_link_clicked")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(true)))
                                .build()));
    }

    /**
     * Creates three vertical expanders inside an accordion and tests expand/collapse functionality.
     */
    @Test
    @MediumTest
    public void testVerticalExpanders() {
        // Regular expander, can expand and collapse.
        ViewProto expanderA =
                (ViewProto) ViewProto.newBuilder()
                        .setVerticalExpanderView(
                                VerticalExpanderViewProto.newBuilder()
                                        .setTitleView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander A Title")))
                                        .setCollapsedView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander A Collapsed")))
                                        .setExpandedView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander A Expanded"))))
                        .build();

        // Only title+collapsed, can not expand (similar to date/time sections in current
        // CollectUserData action).
        ViewProto expanderB =
                (ViewProto) ViewProto.newBuilder()
                        .setVerticalExpanderView(
                                VerticalExpanderViewProto.newBuilder()
                                        .setTitleView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander B Title")))
                                        .setCollapsedView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander B Collapsed")))
                                        .setChevronStyle(
                                                VerticalExpanderViewProto.ChevronStyle.ALWAYS))
                        .build();

        // Regular expander, can expand and collapse.
        ViewProto expanderC =
                (ViewProto) ViewProto.newBuilder()
                        .setVerticalExpanderView(
                                VerticalExpanderViewProto.newBuilder()
                                        .setTitleView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander C Title")))
                                        .setCollapsedView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander C Collapsed")))
                                        .setExpandedView(ViewProto.newBuilder().setTextView(
                                                TextViewProto.newBuilder().setText(
                                                        "Expander C Expanded"))))
                        .build();

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder().setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setExpanderAccordion(
                                                VerticalExpanderAccordionProto.newBuilder()
                                                        .setOrientation(
                                                                LinearLayoutProto.Orientation
                                                                        .VERTICAL))
                                        .addViews(expanderA)
                                        .addViews(expanderB)
                                        .addViews(expanderC)))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Expander A Title"), isDisplayed());

        onView(withText("Expander A Title")).check(matches(isDisplayed()));
        onView(withText("Expander B Title")).check(matches(isDisplayed()));
        onView(withText("Expander C Title")).check(matches(isDisplayed()));

        onView(withText("Expander A Collapsed")).check(matches(isDisplayed()));
        onView(withText("Expander B Collapsed")).check(matches(isDisplayed()));
        onView(withText("Expander C Collapsed")).check(matches(isDisplayed()));

        onView(withText("Expander A Expanded")).check(matches(not(isDisplayed())));
        onView(withText("Expander C Expanded")).check(matches(not(isDisplayed())));

        onView(withText("Expander A Title")).perform(click());
        waitUntilViewMatchesCondition(withText("Expander A Expanded"), isDisplayed());
        onView(withText("Expander C Expanded")).check(matches(not(isDisplayed())));

        onView(withText("Expander C Title")).perform(click());
        waitUntilViewMatchesCondition(withText("Expander C Expanded"), isDisplayed());
        waitUntilViewMatchesCondition(withText("Expander A Expanded"), not(isDisplayed()));

        onView(withText("Expander C Title")).perform(click());
        waitUntilViewMatchesCondition(withText("Expander C Expanded"), not(isDisplayed()));
        onView(withText("Expander A Expanded")).check(matches(not(isDisplayed())));
    }

    /**
     * Tests checkboxes and radio buttons.
     */
    @Test
    @MediumTest
    public void testToggleButtons() {
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        // First radio button group, pre-select nothing.
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_a_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_b_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        // Second radio button group, pre-select option d.
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_c_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_d_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(true)))
                                .build());

        // Pre-select check box e.
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_e_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(true)))
                                .build());
        // Check box is initially unselected
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_f_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        // The only explicit interaction is for the chips. All others are defined implicitly.
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        // Shows two groups of two radio buttons each, as well as a final checkbox.
        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder().setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addAllViews(Arrays.asList(
                                                createRadioButtonView(
                                                        createSimpleTextView("a", "Option A"),
                                                        "group_a", "option_a_view",
                                                        "option_a_toggled"),
                                                createRadioButtonView(
                                                        createSimpleTextView("b", "Option B"),
                                                        "group_a", "option_b_view",
                                                        "option_b_toggled"),
                                                createRadioButtonView(
                                                        createSimpleTextView("c", "Option C"),
                                                        "group_b", "option_c_view",
                                                        "option_c_toggled"),
                                                createRadioButtonView(
                                                        createSimpleTextView("d", "Option D"),
                                                        "group_b", "option_d_view",
                                                        "option_d_toggled"),
                                                createCheckBoxView("Optional option E",
                                                        "option_e_view", "option_e_toggled"),
                                                createCheckBoxView("Optional option F",
                                                        "option_f_view", "option_f_toggled")))))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addAllOutputModelIdentifiers(Arrays.asList(
                                                           "option_a_toggled", "option_b_toggled",
                                                           "option_c_toggled", "option_d_toggled",
                                                           "option_e_toggled", "option_f_toggled")))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());

        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       hasSibling(withText("Option A"))))
                .check(matches(isNotChecked()));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       hasSibling(withText("Option B"))))
                .check(matches(isNotChecked()));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       hasSibling(withText("Option C"))))
                .check(matches(isNotChecked()));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       hasSibling(withText("Option D"))))
                .check(matches(isChecked()));
        onView(allOf(withClassName(is(CheckBox.class.getName())),
                       hasSibling(withText("Optional option E"))))
                .check(matches(isChecked()));
        onView(allOf(withClassName(is(CheckBox.class.getName())),
                       hasSibling(withText("Optional option F"))))
                .check(matches(isNotChecked()));

        onView(withText("Option A")).perform(click());
        onView(withText("Option B")).perform(click());
        onView(withText("Option A")).perform(click());

        // Selecting an already checked radio button does nothing.
        onView(withText("Option C")).perform(click());
        onView(withText("Option C")).perform(click());

        // Selecting an already checked check box inverts its value.
        onView(withText("Optional option E")).perform(click());

        onView(withText("Optional option F")).perform(click());

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Done")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(6));
        assertThat(resultModelValues,
                containsInAnyOrder((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                           .setIdentifier("option_a_toggled")
                                           .setValue(ValueProto.newBuilder().setBooleans(
                                                   BooleanList.newBuilder().addValues(true)))
                                           .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_b_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_c_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(true)))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_d_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_e_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("option_f_toggled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(true)))
                                .build()));
    }

    /**
     * Shows two textviews. Repeated clicks on the second one will enable/disable the first one.
     */
    @Test
    @MediumTest
    public void testEnableDisableView() {
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "toggle_view")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("enabled")
                                        .setBooleanNot(BooleanNotProto.newBuilder().setValue(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "enabled")))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "enabled")))
                        .addCallbacks(CallbackProto.newBuilder().setSetViewEnabled(
                                SetViewEnabledProto.newBuilder()
                                        .setViewIdentifier("text_view")
                                        .setEnabled(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "enabled"))))
                        .build());

        // Disable text_view initially.
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("enabled")
                                .setValue(ValueProto.newBuilder().setBooleans(
                                        BooleanList.newBuilder().addValues(false)))
                                .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder().setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(
                                                ViewProto.newBuilder()
                                                        .setIdentifier("toggle_view")
                                                        .setTextView(
                                                                TextViewProto.newBuilder().setText(
                                                                        "toggle view")))
                                        .addViews(
                                                ViewProto.newBuilder()
                                                        .setIdentifier("text_view")
                                                        .setTextView(
                                                                TextViewProto.newBuilder().setText(
                                                                        "text view")))))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("toggle view"), isCompletelyDisplayed());
        onView(withText("text view")).check(matches(not(isEnabled())));

        onView(withText("toggle view")).perform(click());
        onView(withText("text view")).check(matches(isEnabled()));

        onView(withText("toggle view")).perform(click());
        onView(withText("text view")).check(matches(not(isEnabled())));
    }

    /**
     * Runs multiple generic UI actions, one after another. Tests that earlier runs properly clean
     * up after themselves.
     */
    @Test
    @MediumTest
    public void testMultipleActions() {
        List<InteractionProto> interactionsA = new ArrayList<>();
        interactionsA.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactionsA.add((InteractionProto) InteractionProto.newBuilder()
                                  .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                          OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                  "shared_identifier")))
                                  .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                          EndActionProto.newBuilder().setStatus(
                                                  ProcessedActionStatusProto.ACTION_APPLIED)))
                                  .build());

        List<ModelProto.ModelValue> modelValuesA = new ArrayList<>();
        modelValuesA.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Next")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("shared_identifier"))))
                        .build());

        GenericUserInterfaceProto genericUserInterfaceA =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setModel(ModelProto.newBuilder().addAllValues(modelValuesA))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactionsA))
                        .build();

        // Define a different interaction for the second action, but using the same user action
        // identifier as the first action. This tests that the first action has gone out of scope
        // correctly, and the EndAction interaction defined there no longer exists.
        List<InteractionProto> interactionsB = new ArrayList<>();
        interactionsB.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactionsB.add((InteractionProto) InteractionProto.newBuilder()
                                  .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                          OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                  "shared_identifier")))
                                  .addCallbacks(CallbackProto.newBuilder().setShowInfoPopup(
                                          ShowInfoPopupProto.newBuilder().setInfoPopup(
                                                  InfoPopupProto.newBuilder()
                                                          .setText("Info message")
                                                          .setTitle("Title"))))
                                  .build());

        List<ModelProto.ModelValue> modelValuesB = new ArrayList<>();
        modelValuesB.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("shared_identifier"))))
                        .build());

        GenericUserInterfaceProto genericUserInterfaceB =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setModel(ModelProto.newBuilder().addAllValues(modelValuesB))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactionsB))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterfaceA))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterfaceB))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Next"), isCompletelyDisplayed());
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Done")).perform(click());
        onView(withText("Info message")).check(matches(isDisplayed()));
    }

    /**
     * Shows a generic popup which modifies a value in the global user model.
     */
    @Test
    @MediumTest
    public void testNestedGenericPopups() {
        ValueReferenceProto counterValue = (ValueReferenceProto) ValueReferenceProto.newBuilder()
                                                   .setModelIdentifier("counter")
                                                   .build();
        ValueReferenceProto incrementValue = (ValueReferenceProto) ValueReferenceProto.newBuilder()
                                                     .setValue(ValueProto.newBuilder().setInts(
                                                             IntList.newBuilder().addValues(1)))
                                                     .build();
        CallbackProto incrementCounterCallback =
                (CallbackProto) CallbackProto.newBuilder()
                        .setComputeValue(ComputeValueProto.newBuilder()
                                                 .setResultModelIdentifier("counter")
                                                 .setIntegerSum(IntegerSumProto.newBuilder()
                                                                        .addValues(counterValue)
                                                                        .addValues(incrementValue)))
                        .build();

        // Clicking |nested_text_view| will increment |counter| by 1.
        List<InteractionProto> interactions_nested = new ArrayList<>();
        interactions_nested.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "nested_text_view")))
                        .addCallbacks(incrementCounterCallback)
                        .build());
        GenericUserInterfaceProto nestedUi =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(createTextView("click me (nested)", "nested_text_view"))
                        .setInteractions(InteractionsProto.newBuilder().addAllInteractions(
                                interactions_nested))
                        .build();

        // Clicking |root_text_view| will increment |counter| by 1 and open a nested popup.
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                         OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                                 "root_text_view")))
                                 .addCallbacks(incrementCounterCallback)
                                 .addCallbacks(CallbackProto.newBuilder().setShowGenericPopup(
                                         ShowGenericUiPopupProto.newBuilder()
                                                 .setPopupIdentifier("nested_popup")
                                                 .setGenericUi(nestedUi)))
                                 .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("counter")
                                .setValue(ValueProto.newBuilder().setInts(
                                        IntList.newBuilder().addValues(0)))
                                .build());
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder()
                                             .setIdentifier("root_text_view")
                                             .setTextView(TextViewProto.newBuilder().setText(
                                                     "click me (root)")))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addOutputModelIdentifiers("counter"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Open popup, increment counter to 1.
        waitUntilViewMatchesCondition(withText("click me (root)"), isCompletelyDisplayed());
        onView(withText("click me (root)")).perform(click());

        // Click on nested text view, increment counter to 2.
        waitUntilViewMatchesCondition(withText("click me (nested)"), isCompletelyDisplayed());
        onView(withText("click me (nested)")).perform(click());

        // Close popup.
        Espresso.pressBack();

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Done")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(1));
        assertThat(resultModelValues,
                containsInAnyOrder((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                           .setIdentifier("counter")
                                           .setValue(ValueProto.newBuilder().setInts(
                                                   IntList.newBuilder().addValues(2)))
                                           .build()));
    }

    /**
     * Tests text alignments.
     */
    @Test
    @MediumTest
    public void testTextAlignment() {
        ViewProto defaultAlignedView =
                (ViewProto) ViewProto.newBuilder()
                        .setLayoutParams(ViewLayoutParamsProto.newBuilder().setLayoutWidth(
                                ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE))
                        .setTextView(TextViewProto.newBuilder().setText("default-aligned text"))
                        .build();
        ViewProto centerAlignedView =
                (ViewProto) ViewProto.newBuilder()
                        .setLayoutParams(ViewLayoutParamsProto.newBuilder().setLayoutWidth(
                                ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE))
                        .setTextView(TextViewProto.newBuilder()
                                             .setText("center-aligned text")
                                             .setTextAlignment(
                                                     ViewLayoutParamsProto.Gravity.CENTER_VALUE))
                        .build();

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setLayoutParams(
                                                ViewLayoutParamsProto.newBuilder().setLayoutWidth(
                                                        ViewLayoutParamsProto.Size
                                                                .MATCH_PARENT_VALUE))
                                        .setViewContainer(
                                                ViewContainerProto.newBuilder()
                                                        .setLinearLayout(
                                                                LinearLayoutProto.newBuilder()
                                                                        .setOrientation(
                                                                                LinearLayoutProto
                                                                                        .Orientation
                                                                                        .VERTICAL))
                                                        .addViews(defaultAlignedView)
                                                        .addViews(centerAlignedView)))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("default-aligned text"), isCompletelyDisplayed());
        onView(withText("default-aligned text"))
                .check(matches(withTextGravity(Gravity.START | Gravity.TOP)));
        onView(withText("center-aligned text")).check(matches(withTextGravity(Gravity.CENTER)));
    }

    /**
     * Creates and deletes nested UIs. Also tests startup events for nested UIs.
     */
    @Test
    @MediumTest
    public void testCreateNestedUi() {
        GenericUserInterfaceProto nestedUi =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setIdentifier("nested_text_view")
                                        .setTextView(TextViewProto.newBuilder().setModelIdentifier(
                                                "nested_text")))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "text_view_create_nested_on_click")))
                        .addCallbacks(CallbackProto.newBuilder().setCreateNestedUi(
                                CreateNestedGenericUiProto.newBuilder()
                                        .setGenericUiIdentifier("nested_ui_identifier")
                                        .setGenericUi(nestedUi)
                                        .setParentViewIdentifier("nested_ui_container")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("nested_text")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.newBuilder().setStrings(
                                                        StringList.newBuilder().addValues(
                                                                "Hello World"))))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                         OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                                 "text_view_delete_nested_on_click")))
                                 .addCallbacks(CallbackProto.newBuilder().setClearViewContainer(
                                         ClearViewContainerProto.newBuilder().setViewIdentifier(
                                                 "nested_ui_container")))
                                 .build());

        ViewProto nestedUiContainer =
                (ViewProto) ViewProto.newBuilder()
                        .setIdentifier("nested_ui_container")
                        .setViewContainer(ViewContainerProto.newBuilder().setLinearLayout(
                                LinearLayoutProto.newBuilder().setOrientation(
                                        LinearLayoutProto.Orientation.VERTICAL)))
                        .build();
        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(createSimpleTextView(/*identifier = */
                                                "text_view_create_nested_on_click",
                                                /*text = */ "click me to create nested UI"))
                                        .addViews(createSimpleTextView(/*identifier = */
                                                "text_view_delete_nested_on_click",
                                                /*text = */ "click me to delete nested UI"))
                                        .addViews(nestedUiContainer))
                        .build();

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(rootView)
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder().setGenericUserInterface(
                                 genericUserInterface))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Create/delete UI multiple times, to ensure that everything is cleaned up properly.
        for (int i = 0; i < 2; ++i) {
            waitUntilViewMatchesCondition(
                    withText("click me to create nested UI"), isCompletelyDisplayed());
            onView(withText("click me to create nested UI")).perform(click());

            waitUntilViewMatchesCondition(withText("Hello World"), isCompletelyDisplayed());
            onView(withText("click me to delete nested UI")).perform(click());
            waitUntilViewAssertionTrue(
                    withText("Hello World"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        }
    }

    /**
     * Sets a precondition on the DOM and triggers it.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "1172614")
    public void testElementCondition() throws Exception {
        List<InteractionProto> interactions = new ArrayList<>();

        // When touch_area_one_present becomes false, end the action
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "touch_area_one_present")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setBooleanNot(BooleanNotProto.newBuilder().setValue(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "touch_area_one_present")))
                                        .setResultModelIdentifier("end_action")))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "end_action")))
                        .addCallbacks(CallbackProto.newBuilder()
                                              .setEndAction(EndActionProto.newBuilder().setStatus(
                                                      ProcessedActionStatusProto.ACTION_APPLIED))
                                              .setConditionModelIdentifier("end_action"))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setTextView(TextViewProto.newBuilder().setText("Text"))
                                        .setIdentifier("textView"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder())
                        .build();

        SelectorProto touch_area_one =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(
                                SelectorProto.Filter.newBuilder().setCssSelector("#touch_area_one"))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(touch_area_one)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      touch_area_one))))
                         .build());

        ElementCheck touch_area_one_present =
                (ElementCheck) ElementCheck.newBuilder()
                        .setModelIdentifier("touch_area_one_present")
                        .setElementCondition(
                                ElementConditionProto.newBuilder().setMatch(touch_area_one))
                        .build();

        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setShowGenericUi(
                                ShowGenericUiProto.newBuilder()
                                        .setGenericUserInterface(genericUserInterface)
                                        .setPeriodicElementChecks(
                                                PeriodicElementChecks.newBuilder().addElementChecks(
                                                        touch_area_one_present)))
                        .build());

        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Text"), isCompletelyDisplayed());
        onView(withText("Prompt")).check(doesNotExist());
        tapElement(mTestRule, "touch_area_one");
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }

    /**
     * Tests a simple for-each loop.
     */
    @Test
    @MediumTest
    public void testForEach() {
        // Clicking the view will run a for-each loop that loops over a value and writes the i'th
        // value to result_i, and then ends the action.
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                        "clickable_view")))
                        .addCallbacks(CallbackProto.newBuilder().setForEach(
                                ForEachProto.newBuilder()
                                        .setLoopCounter("i")
                                        .setLoopValueModelIdentifier("loop_value")
                                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                                SetModelValueProto.newBuilder()
                                                        .setModelIdentifier("result_${i}")
                                                        .setValue(createValueReference(
                                                                "loop_value[${i}]"))))))
                        .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                EndActionProto.newBuilder().setStatus(
                                        ProcessedActionStatusProto.ACTION_APPLIED)))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("loop_value")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addAllValues(
                                                Arrays.asList("first", "second", "third"))))
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("result_0")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("result_1")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("result_2")
                                .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder()
                                             .setIdentifier("clickable_view")
                                             .setTextView(TextViewProto.newBuilder().setText(
                                                     "Click me")))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setGenericUserInterface(genericUserInterface)
                                                   .addAllOutputModelIdentifiers(Arrays.asList(
                                                           "result_0", "result_1", "result_2")))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Click me"), isCompletelyDisplayed());

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Click me")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(3));
        assertThat(resultModelValues,
                containsInAnyOrder((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                           .setIdentifier("result_0")
                                           .setValue(ValueProto.newBuilder().setStrings(
                                                   StringList.newBuilder().addValues("first")))
                                           .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("result_1")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("second")))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("result_2")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("third")))
                                .build()));
    }

    /**
     * Tests a nested for-each loop.
     */
    @Test
    @MediumTest
    public void testNestedForEach() {
        // In pseudo code:
        // items = {"first", "second", "third"};
        // for (int i = 0; i < items.size(); i++) {
        //   for (int j = 0; j < items.size(); j++) {
        //     result_i_j = items[j];
        //   }
        // }
        //
        // Which should result in:
        // result_0_0 = first
        // result_0_1 = second
        // result_0_2 = third
        // result_1_0 = first
        // ...
        // result_2_2 = third
        //
        CallbackProto nestedForEach =
                (CallbackProto) CallbackProto.newBuilder()
                        .setForEach(ForEachProto.newBuilder()
                                            .setLoopCounter("j")
                                            .setLoopValueModelIdentifier("loop_value")
                                            .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                                    SetModelValueProto.newBuilder()
                                                            .setModelIdentifier("result_${i}_${j}")
                                                            .setValue(createValueReference(
                                                                    "loop_value[${j}]")))))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                         OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                                 "clickable_view")))
                                 .addCallbacks(CallbackProto.newBuilder().setForEach(
                                         ForEachProto.newBuilder()
                                                 .setLoopCounter("i")
                                                 .setLoopValueModelIdentifier("loop_value")
                                                 .addCallbacks(nestedForEach)))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        List<String> loopValue = Arrays.asList("first", "second", "third");
        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("loop_value")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addAllValues(loopValue)))
                                .build());

        List<String> outputModelIdentifiers = new ArrayList<>();
        for (int i = 0; i < loopValue.size(); i++) {
            for (int j = 0; j < loopValue.size(); j++) {
                String identifier = "result_" + i + "_" + j;
                modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                        .setIdentifier(identifier)
                                        .build());
                outputModelIdentifiers.add(identifier);
            }
        }

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(ViewProto.newBuilder()
                                             .setIdentifier("clickable_view")
                                             .setTextView(TextViewProto.newBuilder().setText(
                                                     "Click me")))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(
                                 ShowGenericUiProto.newBuilder()
                                         .setGenericUserInterface(genericUserInterface)
                                         .addAllOutputModelIdentifiers(outputModelIdentifiers))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Click me"), isCompletelyDisplayed());

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Click me")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();

        List<ModelProto.ModelValue> expectedResultValues = new ArrayList<>();
        for (int i = 0; i < loopValue.size(); i++) {
            for (int j = 0; j < loopValue.size(); j++) {
                expectedResultValues.add(
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("result_" + i + "_" + j)
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues(loopValue.get(j))))
                                .build());
            }
        }
        assertThat(resultModelValues, iterableWithSize(expectedResultValues.size()));
        assertThat(resultModelValues, containsInAnyOrder(expectedResultValues.toArray()));
    }

    /**
     * Shows a simple UI (one view per credit card).
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1102828")
    public void testCreditCardUi() throws Exception {
        // When the toggle button becomes checked, we write the current card to
        // |selected_credit_card|.
        List<InteractionProto> singleCardInteractions = new ArrayList<>();
        singleCardInteractions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "credit_card_selected_${i}")))
                        .addCallbacks(
                                CallbackProto.newBuilder()
                                        .setConditionModelIdentifier("credit_card_selected_${i}")
                                        .setSetValue(
                                                SetModelValueProto.newBuilder()
                                                        .setModelIdentifier("selected_credit_card")
                                                        .setValue(createValueReference(
                                                                "credit_cards[${i}]"))))
                        .build());

        // For each credit card, a simple UI containing the name and the obfuscated number is
        // created.
        GenericUserInterfaceProto singleCardUi =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(createRadioButtonView(
                                (ViewProto) ViewProto.newBuilder()
                                        .setViewContainer(
                                                ViewContainerProto.newBuilder()
                                                        .setLinearLayout(
                                                                LinearLayoutProto.newBuilder()
                                                                        .setOrientation(
                                                                                LinearLayoutProto
                                                                                        .Orientation
                                                                                        .VERTICAL))
                                                        .addViews(createTextModelView(
                                                                "card_holder_name_view_${i}",
                                                                "card_holder_name_${i}"))
                                                        .addViews(createTextModelView(
                                                                "obfuscated_number_view_${i}",
                                                                "obfuscated_number_${i}")))
                                        .build(),
                                "credit_cards", "credit_card_view_${i}",
                                "credit_card_selected_${i}"))
                        .setInteractions(InteractionsProto.newBuilder().addAllInteractions(
                                singleCardInteractions))

                        .build();

        // Every time |credit_cards| changes, we:
        // - clear any previous card views
        // - store previously selected card in 'previously_selected_card'
        // - clear currently selected credit card
        // - compute |card_holder_name_${i}| and |obfuscated_number_${i}|
        // - re-create card UI
        // - try to re-select the previously selected card
        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "credit_cards")))
                        .addCallbacks(CallbackProto.newBuilder().setClearViewContainer(
                                ClearViewContainerProto.newBuilder().setViewIdentifier(
                                        "credit_card_container_view")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("previously_selected_card")
                                        .setValue(createValueReference("selected_credit_card"))))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder()
                                        .setModelIdentifier("selected_credit_card")
                                        .setValue(ValueReferenceProto.newBuilder().setValue(
                                                ValueProto.getDefaultInstance()))))
                        .addCallbacks(CallbackProto.newBuilder().setForEach(
                                ForEachProto.newBuilder()
                                        .setLoopCounter("i")
                                        .setLoopValueModelIdentifier("credit_cards")
                                        .addCallbacks(
                                                createAutofillToStringCallback("credit_cards[${i}]",
                                                        "card_holder_name_${i}", "${51}"))
                                        .addCallbacks(
                                                createAutofillToStringCallback("credit_cards[${i}]",
                                                        "obfuscated_number_${i}", "•••• ${-4}"))
                                        .addCallbacks(CallbackProto.newBuilder().setCreateNestedUi(
                                                CreateNestedGenericUiProto.newBuilder()
                                                        .setGenericUiIdentifier("nested_ui_${i}")
                                                        .setGenericUi(singleCardUi)
                                                        .setParentViewIdentifier(
                                                                "credit_card_container_view")))
                                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                                ComputeValueProto.newBuilder()
                                                        .setResultModelIdentifier(
                                                                "credit_card_selected_${i}")
                                                        .setComparison(createValueComparison(
                                                                createValueReference(
                                                                        "credit_cards[${i}]"),
                                                                createValueReference(
                                                                        "previously_selected_card"),
                                                                ValueComparisonProto.Mode.EQUAL))))

                                        ))
                        .build());

        // Every time |selected_credit_card| changes:
        //  - write the network of the selected card to |selected_card_network|, which will be
        // sent back to backend.
        // - enable/disable confirm button
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "selected_credit_card")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("selected_card_network")
                                        .setCreateCreditCardResponse(
                                                CreateCreditCardResponseProto.newBuilder().setValue(
                                                        createValueReference(
                                                                "selected_credit_card")))))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("confirm_button_disabled")
                                        .setComparison(createValueComparison(
                                                createValueReference("selected_credit_card"),
                                                createEmptyValue(),
                                                ValueComparisonProto.Mode.EQUAL))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "confirm_button_disabled")))
                        .addCallbacks(CallbackProto.newBuilder().setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setResultModelIdentifier("confirm_button_enabled")
                                        .setBooleanNot(BooleanNotProto.newBuilder().setValue(
                                                ValueReferenceProto.newBuilder(createValueReference(
                                                        "confirm_button_disabled"))))))
                        .build());
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "confirm_button_enabled")))
                        .addCallbacks(CallbackProto.newBuilder().setToggleUserAction(
                                ToggleUserActionProto.newBuilder()
                                        .setUserActionsModelIdentifier("chips")
                                        .setUserActionIdentifier("done_chip")
                                        .setEnabled(ValueReferenceProto.newBuilder(
                                                createValueReference("confirm_button_enabled")))))
                        .build());

        // A confirm chip interaction that ends the action.
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("selected_card_network")
                                .build());
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Confirm")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setIdentifier("credit_card_container_view")
                                        .setViewContainer(
                                                ViewContainerProto.newBuilder().setLinearLayout(
                                                        LinearLayoutProto.newBuilder()
                                                                .setOrientation(
                                                                        LinearLayoutProto
                                                                                .Orientation
                                                                                .VERTICAL))))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowGenericUi(
                                 ShowGenericUiProto.newBuilder()
                                         .setGenericUserInterface(genericUserInterface)
                                         .setRequestCreditCards(
                                                 ShowGenericUiProto.RequestAutofillCreditCards
                                                         .newBuilder()
                                                         .setModelIdentifier("credit_cards"))
                                         .addOutputModelIdentifiers("selected_card_network"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        // John with Visa, Jane with MasterCard.
        mHelper.addDummyCreditCard(
                mHelper.addDummyProfile("John Doe", "johndoe@google.com"), "4111111111111111");
        String janeCardId = mHelper.addDummyCreditCard(
                mHelper.addDummyProfile("Jane Doe", "janedoe@google.com"), "5555555555554444");

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("John Doe"), isCompletelyDisplayed());
        onView(withText("Jane Doe")).check(matches(isDisplayed()));
        onView(allOf(withText(containsString("1111")), hasSibling(withText("John Doe"))))
                .check(matches(isDisplayed()));
        onView(allOf(withText(containsString("4444")), hasSibling(withText("Jane Doe"))))
                .check(matches(isDisplayed()));

        // No initial selection.
        onView(withText("John Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        onView(withText("Jane Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        waitUntilViewAssertionTrue(withContentDescription("Confirm"), matches(not(isEnabled())),
                DEFAULT_MAX_TIME_TO_POLL);

        // Select John's card.
        onView(withText("John Doe")).perform(click());
        onView(withText("John Doe")).check(matches(isDescendantOfA(hasSibling(isChecked()))));
        onView(withText("Jane Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        waitUntilViewAssertionTrue(
                withContentDescription("Confirm"), matches(isEnabled()), DEFAULT_MAX_TIME_TO_POLL);

        // Select Jane's card.
        onView(withText("Jane Doe")).perform(click());
        onView(withText("John Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        onView(withText("Jane Doe")).check(matches(isDescendantOfA(hasSibling(isChecked()))));
        waitUntilViewAssertionTrue(
                withContentDescription("Confirm"), matches(isEnabled()), DEFAULT_MAX_TIME_TO_POLL);

        // External: add Mary's card (American Express). Jane's card should still be selected.
        mHelper.addDummyCreditCard(
                mHelper.addDummyProfile("Mary Doe", "marydoe@google.com"), "371449635398431");
        waitUntilViewMatchesCondition(
                allOf(withText(containsString("8431")), hasSibling(withText("Mary Doe"))),
                isDisplayed());
        onView(withText("John Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        onView(withText("Jane Doe")).check(matches(isDescendantOfA(hasSibling(isChecked()))));
        onView(withText("Mary Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        waitUntilViewAssertionTrue(
                withContentDescription("Confirm"), matches(isEnabled()), DEFAULT_MAX_TIME_TO_POLL);

        // External: remove Jane's card. The card selection is cleared.
        mHelper.deleteCreditCard(janeCardId);
        waitUntilViewAssertionTrue(withText("Jane Doe"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(withText("John Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        onView(withText("Mary Doe")).check(matches(isDescendantOfA(hasSibling(isNotChecked()))));
        waitUntilViewAssertionTrue(withContentDescription("Confirm"), matches(not(isEnabled())),
                DEFAULT_MAX_TIME_TO_POLL);

        // Select Mary's card and complete the action.
        onView(withText("Mary Doe")).perform(click());
        waitUntilViewAssertionTrue(
                withContentDescription("Confirm"), matches(isEnabled()), DEFAULT_MAX_TIME_TO_POLL);

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Confirm")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ShowGenericUiProto.Result result = processedActions.get(0).getShowGenericUiResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(1));
        assertThat(resultModelValues,
                containsInAnyOrder(
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("selected_card_network")
                                .setValue(ValueProto.newBuilder().setCreditCardResponse(
                                        CreditCardResponseProto.newBuilder().setNetwork("amex")))
                                .build()));
    }
}
