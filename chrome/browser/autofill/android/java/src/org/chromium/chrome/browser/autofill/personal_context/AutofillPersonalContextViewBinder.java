// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.personal_context;

import static org.chromium.chrome.browser.autofill.personal_context.AutofillPersonalContextProperties.ON_MANAGE_CONNECTED_APPS_CLICKED;
import static org.chromium.chrome.browser.autofill.personal_context.AutofillPersonalContextProperties.ON_PERSONAL_CONTEXT_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.personal_context.AutofillPersonalContextProperties.PERSONAL_CONTEXT_ENABLED;

import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for Autofill Personal Context settings. */
@NullMarked
class AutofillPersonalContextViewBinder {
    public static void bind(
            PropertyModel model, AutofillPersonalContextFragment view, PropertyKey key) {
        if (key == PERSONAL_CONTEXT_ENABLED) {
            ChromeSwitchPreference switchPref = view.getAutofillPersonalContextSwitch();
            if (switchPref != null) {
                switchPref.setChecked(model.get(PERSONAL_CONTEXT_ENABLED));
            }
            // TODO(crbug.com/531644749): Update enterprise policy message visibility in the
            // personal context notice preference when the toggle changes.
        } else if (key == ON_PERSONAL_CONTEXT_TOGGLE_CHANGED) {
            ChromeSwitchPreference switchPref = view.getAutofillPersonalContextSwitch();
            if (switchPref != null) {
                switchPref.setOnPreferenceChangeListener(
                        (preference, newValue) -> {
                            model.get(ON_PERSONAL_CONTEXT_TOGGLE_CHANGED)
                                    .onResult((boolean) newValue);
                            return true;
                        });
            }
        } else if (key == ON_MANAGE_CONNECTED_APPS_CLICKED) {
            Preference pref = view.getAutofillPersonalContextManageConnectedApps();
            if (pref != null) {
                pref.setOnPreferenceClickListener(
                        preference -> {
                            model.get(ON_MANAGE_CONNECTED_APPS_CLICKED).run();
                            return true;
                        });
            }
        }
    }

    private AutofillPersonalContextViewBinder() {}
}
