// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.AUTOFILL_AI_REAUTH_SETTING_ON;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.AUTOFILL_AI_SETTING_ELIGIBLE;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.AUTOFILL_AI_SETTING_ON;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.AUTOFILL_AI_VISIBLE;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.FRAGMENT_TITLE;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_AUTOFILL_AI_REAUTH_SETTING_TOGGLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_AUTOFILL_AI_SETTING_TOGGLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_HINT;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
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
        if (key == FRAGMENT_TITLE) {
            view.getPageTitle().set(model.get(FRAGMENT_TITLE));
        } else if (key == THIRD_PARTY_AUTOFILL_ENABLED) {
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
        } else if (key == AUTOFILL_AI_SETTING_ELIGIBLE) {
            view.getAutofillAiSwitch().setEnabled(model.get(AUTOFILL_AI_SETTING_ELIGIBLE));
        } else if (key == AUTOFILL_AI_SETTING_ON) {
            view.getAutofillAiSwitch().setChecked(model.get(AUTOFILL_AI_SETTING_ON));
        } else if (key == ON_AUTOFILL_AI_SETTING_TOGGLED) {
            view.getAutofillAiSwitch()
                    .setOnPreferenceChangeListener(
                            (preference, newValue) -> {
                                model.get(ON_AUTOFILL_AI_SETTING_TOGGLED)
                                        .onResult((boolean) newValue);
                                return true;
                            });
        } else if (key == AUTOFILL_AI_REAUTH_SETTING_ON) {
            view.getAutofillAiAuthenticationSwitch()
                    .setChecked(model.get(AUTOFILL_AI_REAUTH_SETTING_ON));
        } else if (key == ON_AUTOFILL_AI_REAUTH_SETTING_TOGGLED) {
            view.getAutofillAiAuthenticationSwitch()
                    .setOnPreferenceChangeListener(
                            (preference, newValue) -> {
                                model.get(ON_AUTOFILL_AI_REAUTH_SETTING_TOGGLED)
                                        .onResult((boolean) newValue);
                                return true;
                            });
        } else if (key == AUTOFILL_AI_VISIBLE) {
            boolean visible = model.get(AUTOFILL_AI_VISIBLE);

            Preference aiCategory = view.getAutofillAiCategory();
            if (aiCategory != null) {
                aiCategory.setVisible(visible);
            }
            Preference serviceProviderTitlePreference = view.getAutofillServiceProviderCategory();
            if (serviceProviderTitlePreference != null) {
                serviceProviderTitlePreference.setVisible(visible);
            }
        } else {
            assert false : "Unhandled property: " + key;
        }
    }

    private AutofillOptionsViewBinder() {}
}
