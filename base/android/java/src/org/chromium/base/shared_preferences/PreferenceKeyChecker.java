// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

/**
 * A SharedPreferences key checker that may check if the key is in use.
 *
 * In official builds, {@link NoOpPreferenceKeyChecker} is used, which is a no-op.
 * In debug builds, {@link StrictPreferenceKeyChecker} is used, which checks if a key is registered.
 */
interface PreferenceKeyChecker {
    // Asserts that the SharedPreferences |key| is registered as "in use".
    void checkIsKeyInUse(String key);

    // Asserts that the SharedPreferences KeyPrefix |prefix| is registered as "in use".
    void checkIsPrefixInUse(KeyPrefix prefix);
}
