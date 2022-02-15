// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for PrivacySandboxBridge.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class PrivacySandboxBridgeTest {
    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    @Test
    @SmallTest
    public void testToggleSandboxSetting() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrivacySandboxBridge.setPrivacySandboxEnabled(false);
            assertFalse(PrivacySandboxBridge.isPrivacySandboxEnabled());
            PrivacySandboxBridge.setPrivacySandboxEnabled(true);
            assertTrue(PrivacySandboxBridge.isPrivacySandboxEnabled());
        });
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
}
