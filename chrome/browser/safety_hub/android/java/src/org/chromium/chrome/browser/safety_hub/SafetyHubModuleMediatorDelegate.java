// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;

/**
 * Used by {@link SafetyHubModuleMediator}s to request navigational changes to {@link
 * SafetyHubFragment}.
 */
@NullMarked
interface SafetyHubModuleMediatorDelegate {
    void onUpdateNeeded();

    void showSnackbarForModule(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData);

    void startSettingsForModule(Class<? extends Fragment> fragment);

    void launchSiteSettingsActivityForModule(@SiteSettingsCategory.Type int category);
}
