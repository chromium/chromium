// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;

/**
 * XR extension of WebappActivityTestRule. Applies WebappActivityTestRule then opens
 * up a WebappActivity to a blank page.
 */
public class WebappActivityXrTestRule extends WebappActivityTestRule implements XrTestRule {
    private boolean mTrackerDirty;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                startWebappActivity();
                base.evaluate();
            }
        }, desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.WAA;
    }
}
