// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addSetValueSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GeneratePasswordForFormFieldProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PasswordManagerValue;
import org.chromium.chrome.browser.autofill_assistant.proto.PasswordManagerValue.CredentialType;
import org.chromium.chrome.browser.autofill_assistant.proto.PresaveGeneratedPasswordProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SaveGeneratedPasswordProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextValue;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordChangeLauncher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Integration tests for password change flows.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPasswordManagerIntegrationTest {
    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, "form_target_website.html"));

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    /**
     * Helper function to start a password change flow.
     */
    private void startPasswordChangeFlow(AutofillAssistantTestScript script, String username) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordChangeLauncher.start(getWebContents().getTopLevelNativeWindow(),
                                getWebContents().getLastCommittedUrl(), username,
                                /*skipLogin=*/false));
    }

    private static TextValue buildUsernameValue() {
        return TextValue.newBuilder()
                .setPasswordManagerValue(PasswordManagerValue.newBuilder().setCredentialType(
                        CredentialType.USERNAME))
                .build();
    }

    private static TextValue buildPasswordValue() {
        return TextValue.newBuilder()
                .setPasswordManagerValue(PasswordManagerValue.newBuilder().setCredentialType(
                        CredentialType.PASSWORD))
                .build();
    }

    private static TextValue buildClientMemoryValue(String clientMemoryKey) {
        return TextValue.newBuilder().setClientMemoryKey(clientMemoryKey).build();
    }

    /**
     * Run a password change flow (fill a form with username, old password, new
     * password).
     */
    @Test
    @MediumTest
    public void testPasswordChangeFlow() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        // Sets username
        addSetValueSteps(toCssSelector("#username"), buildUsernameValue(), list);
        // Generates new password
        list.add(ActionProto.newBuilder()
                         .setGeneratePasswordForFormField(
                                 GeneratePasswordForFormFieldProto.newBuilder()
                                         .setMemoryKey("memory-key")
                                         .setElement(toCssSelector("#new-password")))
                         .build());

        // Presaves generated password
        list.add(ActionProto.newBuilder()
                         .setPresaveGeneratedPassword(
                                 PresaveGeneratedPasswordProto.newBuilder().setMemoryKey(
                                         "memory-key"))
                         .build());

        // Sets new password
        addSetValueSteps(
                toCssSelector("#new-password"), buildClientMemoryValue("memory-key"), list);

        // Sets password confirmation
        addSetValueSteps(
                toCssSelector("#password-conf"), buildClientMemoryValue("memory-key"), list);

        // Saves generated password
        list.add(ActionProto.newBuilder()
                         .setSaveGeneratedPassword(
                                 SaveGeneratedPasswordProto.newBuilder().setMemoryKey("memory-key"))
                         .build());

        // Fills login password field with saved password
        addSetValueSteps(toCssSelector("#login-password"), buildPasswordValue(), list);

        // Shows prompt
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        String username = "test_username";
        startPasswordChangeFlow(script, username);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "username"), is(username));
        String password = getElementValue(getWebContents(), "new-password");
        String confirmation_password = getElementValue(getWebContents(), "password-conf");
        String saved_password = getElementValue(getWebContents(), "login-password");
        assertThat(password.length(), greaterThan(0));
        assertThat(password, is(confirmation_password));
        assertThat(saved_password.length(), greaterThan(0));
        assertThat(saved_password, is(password));
    }
}
