// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.autofill.SubKeyRequester;

import java.util.concurrent.TimeoutException;

/** Integration tests for SubKeyRequesterFactory/SubKeyRequester. */
@RunWith(BaseJUnit4ClassRunner.class)
public class SubKeyRequesterIntegrationTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private SubKeyRequester mSubKeyRequester;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSubKeyRequester = SubKeyRequesterFactory.getInstance();
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testLoadSubKeysForRegion() {
        // Trivial test to ensure this API does not crash.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSubKeyRequester.loadRulesForSubKeys("CA");
                });
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSubKeyRequester.getRegionSubKeys("MX", delegate);
                });
        callbackHelper.waitForOnly();
    }
}
