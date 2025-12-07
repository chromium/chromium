// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View Binder for the {@link SafetyHubModuleProperties}. */
@NullMarked
public class SafetyHubModuleViewBinder {
    public static void bindProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        if (SafetyHubModuleProperties.TITLE == propertyKey) {
            preference.setTitle(model.get(SafetyHubModuleProperties.TITLE));
        } else if (SafetyHubModuleProperties.SUMMARY == propertyKey) {
            preference.setSummary(model.get(SafetyHubModuleProperties.SUMMARY));
        } else if (SafetyHubModuleProperties.PRIMARY_BUTTON_TEXT == propertyKey) {
            preference.setPrimaryButtonText(
                    model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_TEXT));
        } else if (SafetyHubModuleProperties.SECONDARY_BUTTON_TEXT == propertyKey) {
            preference.setSecondaryButtonText(
                    model.get(SafetyHubModuleProperties.SECONDARY_BUTTON_TEXT));
        } else if (SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER == propertyKey) {
            preference.setPrimaryButtonClickListener(
                    model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER));
        } else if (SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER == propertyKey) {
            preference.setSecondaryButtonClickListener(
                    model.get(SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER));
        } else if (SafetyHubModuleProperties.ICON == propertyKey) {
            preference.setIcon(model.get(SafetyHubModuleProperties.ICON));
        } else if (SafetyHubModuleProperties.HAS_PROGRESS_BAR == propertyKey) {
            preference.setHasProgressBar(model.get(SafetyHubModuleProperties.HAS_PROGRESS_BAR));
        } else if (SafetyHubModuleProperties.ORDER == propertyKey) {
            preference.setOrder(model.get(SafetyHubModuleProperties.ORDER));
        } else if (SafetyHubModuleProperties.IS_VISIBLE == propertyKey) {
            preference.setVisible(model.get(SafetyHubModuleProperties.IS_VISIBLE));
        } else if (SafetyHubModuleProperties.IS_EXPANDED == propertyKey) {
            preference.setExpanded(model.get(SafetyHubModuleProperties.IS_EXPANDED));
        } else {
            assert false : "Unhandled property detected in SafetyHubModuleViewBinder";
        }
    }
}
