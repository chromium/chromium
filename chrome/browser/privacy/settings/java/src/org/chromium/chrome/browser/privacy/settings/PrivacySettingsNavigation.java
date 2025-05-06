// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import org.chromium.build.annotations.NullMarked;

/** Arguments for {@link PrivacySettings} {@link SettingsNavigation}. */
@NullMarked
public class PrivacySettingsNavigation {
    // Extra for intent to launch PrivacySettings fragment. Extra indicates that PrivacySettings
    // should scroll to "advanced-protection" section after launching PrivacySettings.
    public static final String EXTRA_FOCUS_ADVANCED_PROTECTION_SECTION =
            "focus_advanced_protection_section";
}
