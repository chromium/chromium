// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * XR extension of ChromeTabbedActivityTestRule. Applies ChromeTabbedActivityTestRule then opens up
 * a ChromeTabbedActivity to a blank page.
 */
public class ChromeTabbedActivityXrTestRule extends ChromeTabbedActivityTestRule
        implements XrTestRule {
    @Override
    protected void before() throws Throwable {
        super.before();
        startMainActivityOnBlankPage();
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.CTA;
    }
}
