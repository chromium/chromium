// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import android.support.test.InstrumentationRegistry;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;

/**
 * VR extension of CustomTabActivityTestRule. Applies CustomTabActivityTestRule then
 * opens up a CustomTabActivity to a blank page while performing some additional VR-only setup.
 */
public class CustomTabActivityVrTestRule extends CustomTabActivityTestRule implements VrTestRule {
    private boolean mDonEnabled;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                VrTestRuleUtils.evaluateVrTestRuleImpl(
                        base, desc, CustomTabActivityVrTestRule.this, () -> {
                            startCustomTabActivityWithIntent(
                                    VrTestRuleUtils.maybeAddStandaloneIntentData(
                                            CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                                    InstrumentationRegistry.getTargetContext(),
                                                    "about:blank")));
                            TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                        });
            }
        }, desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.CCT;
    }

    @Override
    public boolean isDonEnabled() {
        return CommandLine.getInstance().hasSwitch("vr-don-enabled");
    }
}
