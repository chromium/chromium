// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

/** A placeholder key checker that never throws exceptions. Used in production builds. */
class NoOpPreferenceKeyChecker implements PreferenceKeyChecker {
    @Override
    public void checkIsKeyInUse(String key) {
        // No-op.
    }

    @Override
    public void checkIsPrefixInUse(KeyPrefix prefix) {
        // No-op.
    }
}
