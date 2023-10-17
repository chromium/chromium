// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;

/**
 * Interface to be implemented by *XrTestRule rules, which allows them to be conditionally skipped
 * when used in conjunction with XrActivityRestrictionRule.
 */
public interface XrTestRule {
    /**
     * @return The XrActivityRestriction.SupportedActivity that this rule is restricted to running
     *     in.
     */
    public @SupportedActivity int getRestriction();
}
