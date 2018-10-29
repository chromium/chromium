// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * XR extension of ChromeTabbedActivityTestRule. Applies ChromeTabbedActivityTestRule
 * then opens up a ChromeTabbedActivity to a blank page.
 */
public class ChromeTabbedActivityXrTestRule
        extends ChromeTabbedActivityTestRule implements XrTestRule {
    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                startMainActivityOnBlankPage();
                base.evaluate();
            }
        }, desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.CTA;
    }
}
