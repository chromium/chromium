// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/** Interface for the extension search engine coordinator. */
@NullMarked
public interface ExtensionSearchEngineCoordinator extends Destroyable {
    @Initializer
    void initialize(
            Context context,
            Profile profile,
            SearchEngineListPreference pref,
            SettingsCustomTabLauncher settingsCustomTabLauncher);
}
