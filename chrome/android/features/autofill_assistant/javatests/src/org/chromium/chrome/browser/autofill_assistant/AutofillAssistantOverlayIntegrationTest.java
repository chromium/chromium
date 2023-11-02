// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementOnScreen;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getBitmapFromView;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toIFrameCssSelector;

import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.BitmapDrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientDimensionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto.OverlayBehavior;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.OverlayImageProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.R;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant's overlay.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantOverlayIntegrationTest {
    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain =
            RuleChain.outerRule(mTestRule).around(new AutofillAssistantCustomTabTestRule(
                    mTestRule, "autofill_assistant_target_website.html"));

    /**
     * Tests that clicking on a document element works with a showcast.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testShowCastOnDocumentElement() throws Exception {
        SelectorProto element = toCssSelector("#touch_area_one");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_one"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "touch_area_four");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
    }

    /**
     * Showcasts the same element twice in a row, and taps it the second time. Tests that the second
     * showcast works correctly, even though the showcasted area hasn't changed (see b/161471176).
     */
    @Test
    @MediumTest
    public void testRepeatedShowCastOnSameElement() throws Exception {
        SelectorProto element = toCssSelector("#touch_area_one");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("First Prompt")
                                            .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Second Prompt")
                                            .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Done"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_one"));
        waitUntilViewMatchesCondition(withText("First Prompt"), isCompletelyDisplayed());
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Second Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "touch_area_four");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
    }

    /**
     * Tests that clicking on a document element requiring scrolling works with a showcast.
     */
    @Test
    @MediumTest
    public void testShowCastOnDocumentElementInScrolledBrowserWindow() throws Exception {
        SelectorProto element = toCssSelector("#touch_area_five");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_five"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM. The element should be after a
        // big element forcing the page to scroll.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_five"), is(true));
        tapElement(mTestRule, "touch_area_five");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_five"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "touch_area_six");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_six"), is(true));
    }

    /**
     * Tests that clicking on an iFrame element works with a showcast.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1272997")
    public void testShowCastOnIFrameElement() throws Exception {
        SelectorProto element = toIFrameCssSelector("#iframe", "#touch_area_1");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "iframe", "touch_area_1"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM.
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_1"), is(true));
        tapElement(mTestRule, "iframe", "touch_area_1");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_1"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "iframe", "touch_area_2");
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_2"), is(true));
    }

    /**
     * Tests that clicking on an iFrame element works with a showcast in a scrolled iFrame.
     */
    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.Q,
            message = "Flaky on Android 11: https://crbug.com/1276995")
    public void
    testShowCastOnIFrameElementInScrollIFrame() throws Exception {
        SelectorProto element = toIFrameCssSelector("#iframe", "#touch_area_3");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "iframe", "touch_area_3"));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Tapping on the element should remove it from the DOM. The element should be after a
        // big element forcing the page to scroll.
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_3"), is(true));
        tapElement(mTestRule, "iframe", "touch_area_3");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_3"));
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "iframe", "touch_area_4");
        assertThat(
                checkElementExists(mTestRule.getWebContents(), "iframe", "touch_area_4"), is(true));
    }

    /**
     * Tests that three taps on the scrim removes the scrim and hides the Autofill Assistant.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "Flaky - https://crbug.com/1269961")
    public void testThreeClicksHideAssistant() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(
                                 PromptProto.newBuilder()
                                         .setMessage("Overlay present")
                                         .addChoices(Choice.newBuilder().setChip(
                                                 ChipProto.newBuilder()
                                                         .setType(org.chromium.chrome.browser
                                                                          .autofill_assistant.proto
                                                                          .ChipType.DONE_ACTION)
                                                         .setText("Hide"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_one"));
        waitUntilViewMatchesCondition(withText("Overlay present"), isCompletelyDisplayed());

        // Tapping the scrim three times should hide the scrim and the Autofill Assistant.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        tapElement(mTestRule, "touch_area_four");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        tapElement(mTestRule, "touch_area_four");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        tapElement(mTestRule, "touch_area_four");

        // Afterwards the scrim and Autofill Assistant are removed.
        waitUntil(()
                          -> mTestRule.getActivity()
                                     .getRootUiCoordinatorForTesting()
                                     .getScrimCoordinator()
                                     .getViewForTesting()
                        == null);
        waitUntilViewAssertionTrue(
                withId(R.id.autofill_assistant), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
    }

    /**
     * Tests that changing the OverlayBehavior setting affects the overlay as intended.
     */
    @Test
    @MediumTest
    public void testOverlayBehaviorSetting() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(
                                 PromptProto.newBuilder()
                                         .setMessage("Overlay present")
                                         .addChoices(Choice.newBuilder().setChip(
                                                 ChipProto.newBuilder()
                                                         .setType(org.chromium.chrome.browser
                                                                          .autofill_assistant.proto
                                                                          .ChipType.DONE_ACTION)
                                                         .setText("Hide"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                 OverlayBehavior.HIDDEN))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(
                                 PromptProto.newBuilder()
                                         .setMessage("Overlay hidden")
                                         .addChoices(Choice.newBuilder().setChip(
                                                 ChipProto.newBuilder()
                                                         .setType(org.chromium.chrome.browser
                                                                          .autofill_assistant.proto
                                                                          .ChipType.DONE_ACTION)
                                                         .setText("Default"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                 OverlayBehavior.DEFAULT))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Overlay present")
                                            .addChoices(PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_one"));
        waitUntilViewMatchesCondition(withText("Overlay present"), isCompletelyDisplayed());

        // Tapping the element should not do anything  since the overlay should be present.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        tapElement(mTestRule, "touch_area_one");

        // Go to the next action to hide the overlay.
        onView(withText("Hide")).perform(click());
        // Tapping the item should now hide it.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // Go to the next action to set the overlay to full again.
        onView(withText("Default")).perform(click());
        // Tapping on the element should be blocked by the overlay.
        tapElement(mTestRule, "touch_area_four");
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
    }

    /**
     * Tests that an image works with an overlay
     */
    @Test
    @MediumTest
    public void testShowImageOnOverlay() throws Exception {
        String redDotBase64Url =
                "data:image/png;base64, iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
        int imageSizeInPixel = 50;
        ClientSettingsProto clientSettings =
                ClientSettingsProto.newBuilder()
                        .setOverlayImage(
                                OverlayImageProto.newBuilder()
                                        .setImageDrawable(DrawableProto.newBuilder().setBitmap(
                                                BitmapDrawableProto.newBuilder()
                                                        .setUrl(redDotBase64Url)
                                                        .setWidth(ClientDimensionProto.newBuilder()
                                                                          .setSizeInPixel(
                                                                                  imageSizeInPixel))
                                                        .setHeight(
                                                                ClientDimensionProto.newBuilder()
                                                                        .setSizeInPixel(
                                                                                imageSizeInPixel))))
                                        .setImageSize(
                                                ClientDimensionProto.newBuilder().setSizeInPixel(
                                                        imageSizeInPixel)))
                        .build();

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                new ArrayList<>());
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script), clientSettings);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        ViewGroup chromeCoordinatorView = mTestRule.getActivity().findViewById(R.id.coordinator);
        View scrim = mTestRule.getActivity()
                             .getRootUiCoordinatorForTesting()
                             .getScrimCoordinator()
                             .getViewForTesting();

        onView(is(scrim)).check(matches(isCompletelyDisplayed()));
        int image_center_x = scrim.getWidth() / 2;
        int yTopContentOffset = mTestRule.getActivity()
                                        .getRootUiCoordinatorForTesting()
                                        .getBrowserControlsManager()
                                        .getContentOffset();
        int image_center_y = yTopContentOffset + imageSizeInPixel / 2;

        // Testing that central pixel of overlay image is different from (0,0) pixel
        waitUntil(()
                          -> getBitmapFromView(scrim).getPixel(image_center_x, image_center_y)
                        != getBitmapFromView(scrim).getPixel(0, 0));
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}
