// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Fragment containing enhanced protection settings.
 */
public class EnhancedProtectionSettingsFragment extends SafeBrowsingSettingsFragmentBase {
    @Override
    protected int getPreferenceResource() {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)) {
            return R.xml.enhanced_protection_preferences_updated;
        } else {
            return R.xml.enhanced_protection_preferences;
        }
    }
}
