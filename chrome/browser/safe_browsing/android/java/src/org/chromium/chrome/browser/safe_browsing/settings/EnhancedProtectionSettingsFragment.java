// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

/**
 * Fragment containing enhanced protection settings.
 */
public class EnhancedProtectionSettingsFragment extends SafeBrowsingSettingsFragmentBase {
    @Override
    protected int getPreferenceResource() {
        return R.xml.enhanced_protection_preferences;
    }
}
