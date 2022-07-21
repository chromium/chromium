// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.BitmapDrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientDimensionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigBasedUrlProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoBoxProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowInfoBoxProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.R;

import java.util.ArrayList;
import java.util.Collections;

/** Tests autofill assistant's show info box feature. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantInfoBoxEndToEndTest {
    private static final String TEST_PAGE = "form_target_website.html";
    private static final String IMAGE_URL =
            "data:png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAQAAABKfvVzAAAAGUlEQVR4AWMgAYyC/4QhmRoQYNTTo54eBQD1KGuVIr+14AAAAABJRU5ErkJggg==";
    private static final ClientDimensionProto DIMENSION =
            ClientDimensionProto.newBuilder().setDp(150).build();
    private static final ConfigBasedUrlProto MDPI_CONFIG =
            ConfigBasedUrlProto.newBuilder().putUrl("mdpi", IMAGE_URL).build();

    private static final ConfigBasedUrlProto DARK_MDPI_CONFIG =
            ConfigBasedUrlProto.newBuilder().putUrl("night-mdpi", IMAGE_URL).build();

    private static DrawableProto drawable(ConfigBasedUrlProto config) {
        BitmapDrawableProto.Builder bitmapBuilder = BitmapDrawableProto.newBuilder();
        bitmapBuilder.setConfigBasedUrl(config);
        bitmapBuilder.setHeight(DIMENSION);
        bitmapBuilder.setWidth(DIMENSION);
        return DrawableProto.newBuilder().setBitmap(bitmapBuilder.build()).build();
    }

    private static ShowInfoBoxProto showInfoBox(DrawableProto drawable) {
        return ShowInfoBoxProto.newBuilder()
                .setInfoBox(InfoBoxProto.newBuilder().setDrawable(drawable))
                .build();
    }

    private static ShowInfoBoxProto showInfoBox(String explanation) {
        return ShowInfoBoxProto.newBuilder()
                .setInfoBox(InfoBoxProto.newBuilder().setExplanation(explanation))
                .build();
    }

    private static ShowInfoBoxProto showInfoBox(String explanation, DrawableProto drawable) {
        return ShowInfoBoxProto.newBuilder()
                .setInfoBox(
                        InfoBoxProto.newBuilder().setExplanation(explanation).setDrawable(drawable))
                .build();
    }

    private static ActionProto actionProto(ShowInfoBoxProto showInfoBox) {
        return ActionProto.newBuilder().setShowInfoBox(showInfoBox).build();
    }

    private static AutofillAssistantTestScript testScript(ArrayList<ActionProto> list) {
        return new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
    }

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE));

    private void runAutofillAssistant(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }

    private Matcher<View> getImageView() {
        return withId(R.id.info_box_image);
    }

    private Matcher<View> getTextView() {
        return withId(R.id.info_box_explanation);
    }

    @Test
    @MediumTest
    public void showInfoBoxShown_onlyText() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(actionProto(showInfoBox("explanation")));

        runAutofillAssistant(testScript(list));

        waitUntilViewMatchesCondition(withText("explanation"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void showInfoBoxShown_textAndImage() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(actionProto(showInfoBox("explanation", drawable(MDPI_CONFIG))));

        runAutofillAssistant(testScript(list));

        waitUntilViewMatchesCondition(withText("explanation"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(getImageView(), isDisplayed());
    }

    @Test
    @MediumTest
    public void showInfoBoxShown_image() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(actionProto(showInfoBox(drawable(MDPI_CONFIG))));

        runAutofillAssistant(testScript(list));

        waitUntilViewMatchesCondition(getImageView(), isDisplayed());
    }

    @Test
    @MediumTest
    public void showInfoBoxShown_fallbacks() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(actionProto(showInfoBox(drawable(DARK_MDPI_CONFIG))));

        runAutofillAssistant(testScript(list));

        waitUntilViewMatchesCondition(getImageView(), isDisplayed());
    }
}
