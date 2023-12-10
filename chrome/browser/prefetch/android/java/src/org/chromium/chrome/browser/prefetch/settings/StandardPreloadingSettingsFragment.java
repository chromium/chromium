// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefetch.settings;

/** Fragment containing standard preloading settings. */
public class StandardPreloadingSettingsFragment extends PreloadPagesSettingsFragmentBase {
    @Override
    protected int getPreferenceResource() {
        return R.xml.standard_preloading_preferences;
    }
}
