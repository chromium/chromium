// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

/**
 * A dummy key checker that never throws exceptions. Used in production builds.
 */
class BaseChromePreferenceKeyChecker {
    void checkIsKeyInUse(String key) {
        // No-op.
    }

    void checkIsPrefixInUse(KeyPrefix prefix) {
        // No-op.
    }
}
