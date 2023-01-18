// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * VR extension of ChromeTabbedActivityTestRule. Applies ChromeTabbedActivityTestRule
 * then opens up a ChromeTabbedActivity to a blank page while performing some additional VR-only
 * setup.
 */
public class ChromeTabbedActivityVrTestRule
        extends ChromeTabbedActivityTestRule implements VrTestRule {
    private boolean mDonEnabled;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                VrTestRuleUtils.evaluateVrTestRuleImpl(
                        base, desc, ChromeTabbedActivityVrTestRule.this, () -> {
                            startMainActivityOnBlankPage();
                            TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                        });
            }
        }, desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.CTA;
    }

    @Override
    public boolean isDonEnabled() {
        return CommandLine.getInstance().hasSwitch("vr-don-enabled");
    }
}
