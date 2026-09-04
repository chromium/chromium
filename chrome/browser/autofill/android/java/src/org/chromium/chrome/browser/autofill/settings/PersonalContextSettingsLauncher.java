// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;

/** Helper for launching Personal Context/options settings. */
@NullMarked
public class PersonalContextSettingsLauncher {
    private PersonalContextSettingsLauncher() {}

    public static boolean showPersonalContextSettings(
            @Nullable Context context, @AutofillOptionsReferrer int referrer) {
        if (context == null) {
            return false;
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)) {
            SettingsNavigationFactory.createSettingsNavigation(context)
                    .startSettings(
                            context,
                            AutofillPersonalContextFragment.class,
                            /* fragmentArgs= */ null,
                            /* addToBackStack= */ true);
        } else {
            SettingsNavigationFactory.createSettingsNavigation(context)
                    .startSettings(
                            context,
                            AutofillOptionsFragment.class,
                            AutofillOptionsFragment.createRequiredArgs(referrer),
                            /* addToBackStack= */ true);
        }
        return true;
    }
}
