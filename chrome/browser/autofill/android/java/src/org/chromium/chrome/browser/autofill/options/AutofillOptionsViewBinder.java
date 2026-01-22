// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.AUTOFILL_AI_SETTING_VISIBLE;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_HINT;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless view binder of the autofill options component. Whenever a ModelChangeProcessor detects
 * model changes, this class helps to map the model state to the {@link AutofillOptionsFragment}.
 */
@NullMarked
class AutofillOptionsViewBinder {
    /**
     * Bind the changes of {@link AutofillOptionsProperties} to the view of the options component
     * which is the {@link AutofillOptionsFragment}.
     *
     * @param model A {@link PropertyModel} constructed from all {@link AutofillOptionsProperties}.
     * @param view An {@link AutofillOptionsFragment}
     * @param key An {@link AutofillOptionsProperties} key.
     */
    public static void bind(PropertyModel model, AutofillOptionsFragment view, PropertyKey key) {
        if (key == THIRD_PARTY_AUTOFILL_ENABLED) {
            @RadioButtonGroupThirdPartyPreference.ThirdPartyOption
            int currentOption =
                    model.get(THIRD_PARTY_AUTOFILL_ENABLED)
                            ? RadioButtonGroupThirdPartyPreference.ThirdPartyOption
                                    .USE_OTHER_PROVIDER
                            : RadioButtonGroupThirdPartyPreference.ThirdPartyOption.DEFAULT;
            view.getThirdPartyFillingOption().setSelectedOption(currentOption);
        } else if (key == ON_THIRD_PARTY_TOGGLE_CHANGED) {
            view.getThirdPartyFillingOption()
                    .setOnPreferenceChangeListener(
                            (unused, newValue) -> {
                                boolean optedIntoOtherProviders =
                                        (int) newValue
                                                == RadioButtonGroupThirdPartyPreference
                                                        .ThirdPartyOption.USE_OTHER_PROVIDER;
                                model.get(ON_THIRD_PARTY_TOGGLE_CHANGED)
                                        .onResult(optedIntoOtherProviders);
                                return true;
                            });
        } else if (key == THIRD_PARTY_TOGGLE_IS_READ_ONLY) {
            view.getThirdPartyFillingOption()
                    .setEnabled(!model.get(THIRD_PARTY_TOGGLE_IS_READ_ONLY));
        } else if (key == THIRD_PARTY_TOGGLE_HINT) {
            view.getHint().setSummary(model.get(THIRD_PARTY_TOGGLE_HINT));
            view.getHint().setVisible(model.get(THIRD_PARTY_TOGGLE_HINT) != null);
        } else if (key == AUTOFILL_AI_SETTING_VISIBLE) {
            ChromeSwitchPreference autofillAiSwitch = view.getAutofillAiSwitch();
            autofillAiSwitch.setVisible(model.get(AUTOFILL_AI_SETTING_VISIBLE));
            autofillAiSwitch.setTitle(R.string.settings_autofill_ai_page_title);
            autofillAiSwitch.setSummary(R.string.settings_autofill_ai_description);
            autofillAiSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        // TODO(crbug.com/376662762): Implement side effect.
                        return true;
                    });
        } else {
            assert false : "Unhandled property: " + key;
        }
    }

    private AutofillOptionsViewBinder() {}
}
