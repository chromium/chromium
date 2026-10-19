// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;

/** Helper for launching Personal Context settings. */
@NullMarked
public class PersonalContextSettingsLauncher {
    private PersonalContextSettingsLauncher() {}

    public static boolean showPersonalContextSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillPersonalContextFragment.class,
                        /* fragmentArgs= */ null,
                        /* addToBackStack= */ true);
        return true;
    }
}
