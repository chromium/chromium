// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.VrCardboardTestRuleUtils;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;

/**
 * Cardboard extension of WebappActivityTestRule. Applies WebappActivityTestRule then opens up a
 * WebappActivity to a blank page while performing some additional setup.
 */
public class WebappActivityVrCardboardTestRule extends WebappActivityTestRule
        implements VrTestRule {
    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        VrCardboardTestRuleUtils.evaluateVrTestRuleImpl(
                                base,
                                desc,
                                WebappActivityVrCardboardTestRule.this,
                                () -> {
                                    startWebappActivity();
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
        return false;
    }
}
