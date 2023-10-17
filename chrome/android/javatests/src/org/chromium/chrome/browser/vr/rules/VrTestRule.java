// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

/**
 * Interface to be implemented by *VrTestRule rules, which allows them to be conditionally skipped
 * when used in conjunction with XrActivityRestrictionRule and properly set/ clean up the fake head
 * pose tracker.
 */
public interface VrTestRule extends XrTestRule {
    /**
     * Whether the currently applied settings result in the DON flow being enabled.
     *
     * @return True if the DON flow is enabled, false otherwise.
     */
    public boolean isDonEnabled();
}
