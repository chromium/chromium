// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SiteSettingsHelpClient;

/**
 * A SiteSettingsHelpClient instance that provides Chrome-specific help functionality.
 */
public class ChromeSiteSettingsHelpClient implements SiteSettingsHelpClient {
    @Override
    public boolean isHelpAndFeedbackEnabled() {
        return true;
    }

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getInstance().show(currentActivity,
                currentActivity.getString(R.string.help_context_settings),
                Profile.getLastUsedRegularProfile(), null);
    }

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getInstance().show(currentActivity,
                currentActivity.getString(R.string.help_context_protected_content),
                Profile.getLastUsedRegularProfile(), null);
    }
}
