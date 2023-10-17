// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/** Tests for PrivacySandboxBridge. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=PrivacySandboxSettings3:show-sample-data/true"
})
@Batch(Batch.PER_CLASS)
public class PrivacySandboxBridgeTest {
    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    private UserActionTester mUserActionTester;

    @Test
    @SmallTest
    public void testToggleSandboxSetting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxBridge.setPrivacySandboxEnabled(false);
                    assertFalse(PrivacySandboxBridge.isPrivacySandboxEnabled());
                    PrivacySandboxBridge.setPrivacySandboxEnabled(true);
                    assertTrue(PrivacySandboxBridge.isPrivacySandboxEnabled());
                });
    }

    @Test
    @SmallTest
    public void testNoDialogWhenFreDisabled() {
        // Check that when ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE is present, the bridge
        // returns that no dialog is shown. This is important to prevent tests that rely on using
        // that flag to get a blank activity with no dialogs from breaking.
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                "Returned dialog type",
                                PromptType.NONE,
                                PrivacySandboxBridge.getRequiredPromptType()));
    }

    @Test
    @SmallTest
    public void testGetCurrentTopics() {
        // Check that this function returns a valid list. We currently can't control from the Java
        // side what they actually return, so just check that it is not null and there is no crash.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> assertNotNull(PrivacySandboxBridge.getCurrentTopTopics()));
    }

    @Test
    @SmallTest
    public void testBlockedTopics() {
        // Check that this function returns a valid list. We currently can't control from the Java
        // side what they actually return, so just check that it is not null and there is no crash.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> assertNotNull(PrivacySandboxBridge.getBlockedTopics()));
    }

    @Test
    @SmallTest
    public void testFakeTopics() {
        Topic topic1 = new Topic(1, 1, "Arts & entertainment");
        Topic topic2 = new Topic(2, 1, "Acting & theater");
        Topic topic3 = new Topic(3, 1, "Comics");
        Topic topic4 = new Topic(4, 1, "Concerts & music festivals");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(
                            PrivacySandboxBridge.getCurrentTopTopics(), contains(topic2, topic1));
                    assertThat(PrivacySandboxBridge.getBlockedTopics(), contains(topic3, topic4));
                    PrivacySandboxBridge.setTopicAllowed(topic1, false);
                    assertThat(PrivacySandboxBridge.getCurrentTopTopics(), contains(topic2));
                    assertThat(
                            PrivacySandboxBridge.getBlockedTopics(),
                            contains(topic1, topic3, topic4));
                    PrivacySandboxBridge.setTopicAllowed(topic4, true);
                    assertThat(
                            PrivacySandboxBridge.getCurrentTopTopics(), contains(topic2, topic4));
                    assertThat(PrivacySandboxBridge.getBlockedTopics(), contains(topic1, topic3));
                });
    }

    @Nullable
    private List<String> getFledgeJoiningEtlds() {
        PayloadCallbackHelper<List<String>> callbackHelper = new PayloadCallbackHelper<>();
        PrivacySandboxBridge.getFledgeJoiningEtldPlusOneForDisplay(callbackHelper::notifyCalled);
        return callbackHelper.getOnlyPayloadBlocking();
    }

    @Test
    @SmallTest
    public void testGetFledgeJoiningEtldPlusOneForDisplay() {
        // Check that this function returns a valid list. We currently can't control from the Java
        // side what they actually return, so just check that it is not null and there is no crash.
        TestThreadUtils.runOnUiThreadBlocking(() -> assertNotNull(getFledgeJoiningEtlds()));
    }

    @Test
    @SmallTest
    public void testGetBlockedFledgeJoiningTopFramesForDisplay() {
        // Check that this function returns a valid list. We currently can't control from the Java
        // side what they actually return, so just check that it is not null and there is no crash.
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertNotNull(
                                PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay()));
    }

    @Test
    @SmallTest
    public void testFledgeBlocking() {
        String site1 = "a.com";
        String site2 = "b.com";
        String site3 = "c.com";

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxBridge.setFledgeJoiningAllowed(site1, false);
                    assertThat(
                            PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                            contains(site1));

                    PrivacySandboxBridge.setFledgeJoiningAllowed(site2, false);
                    PrivacySandboxBridge.setFledgeJoiningAllowed(site3, false);
                    assertThat(
                            PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                            contains(site1, site2, site3));

                    PrivacySandboxBridge.setFledgeJoiningAllowed(site2, true);
                    assertThat(
                            PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                            contains(site1, site3));
                });
    }

    @Test
    @SmallTest
    public void testPromptActionOccuredRecordsUserAction() {
        mUserActionTester = new UserActionTester();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_SHOWN);
                    assertTrue(
                            mUserActionTester
                                    .getActions()
                                    .contains("Settings.PrivacySandbox.Consent.Shown"));
                });
    }
}
