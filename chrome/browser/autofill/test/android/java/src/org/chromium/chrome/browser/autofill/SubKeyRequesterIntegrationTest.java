// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.autofill.SubKeyRequester;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for SubKeyRequesterFactory/SubKeyRequester.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SubKeyRequesterIntegrationTest {
    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public final TestRule mFeaturesProcessorRule = new Features.InstrumentationProcessor();

    private SubKeyRequester mSubKeyRequester;

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSubKeyRequester = SubKeyRequesterFactory.getInstance(); });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testLoadSubKeysForRegion() {
        // Trivial test to ensure this API does not crash.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSubKeyRequester.loadRulesForSubKeys("CA"); });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetRegionSubKeys_validRegion() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        SubKeyRequester.GetSubKeysRequestDelegate delegate =
                new SubKeyRequester.GetSubKeysRequestDelegate() {
                    @Override
                    public void onSubKeysReceived(String[] subKeysCodes, String[] subKeysNames) {
                        callbackHelper.notifyCalled();
                    }
                };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSubKeyRequester.getRegionSubKeys("MX", delegate); });
        callbackHelper.waitForFirst();
    }
}
