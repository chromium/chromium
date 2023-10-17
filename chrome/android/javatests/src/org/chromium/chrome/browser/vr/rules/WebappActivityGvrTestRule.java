// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.GvrTestRuleUtils;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;

/**
 * VR extension of WebappActivityTestRule. Applies WebappActivityTestRule then opens up a
 * WebappActivity to a blank page while performing some additional VR-only setup.
 */
public class WebappActivityGvrTestRule extends WebappActivityTestRule implements VrTestRule {
    private boolean mDonEnabled;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        GvrTestRuleUtils.evaluateVrTestRuleImpl(
                                base,
                                desc,
                                WebappActivityGvrTestRule.this,
                                () -> {
                                    startWebappActivity();
                                    TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                                });
                    }
                },
                desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.WAA;
    }

    @Override
    public boolean isDonEnabled() {
        return CommandLine.getInstance().hasSwitch("vr-don-enabled");
    }
}
