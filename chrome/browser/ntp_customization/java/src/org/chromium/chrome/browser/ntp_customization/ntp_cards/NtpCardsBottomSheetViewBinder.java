// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.ntp_customization.MaterialSwitchWithTextListContainerView;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the NTP cards bottom sheet. */
@NullMarked
public class NtpCardsBottomSheetViewBinder {
    /**
     * Handles the binding of the bottom sheet for the NTP cards.
     *
     * <p>Expects PropertyModel with keys NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS, and
     * expects the view to be R.layout.ntp_customization_ntp_cards_bottom_sheet.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        MaterialSwitchWithText allCardsSwitch = view.findViewById(R.id.cards_switch_button);
        MaterialSwitchWithTextListContainerView ntpCardsList =
                view.findViewById(R.id.ntp_cards_container);

        if (propertyKey == ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER) {
            if (allCardsSwitch == null) return;

            allCardsSwitch.setOnCheckedChangeListener(
                    model.get(ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER));
        } else if (propertyKey == ARE_CARD_SWITCHES_ENABLED) {
            boolean areCardsEnabled = model.get(ARE_CARD_SWITCHES_ENABLED);
            if (allCardsSwitch != null) {
                allCardsSwitch.setChecked(areCardsEnabled);
            }
            if (ntpCardsList != null) {
                ntpCardsList.setAllModuleSwitchesEnabled(areCardsEnabled);
            }
        } else if (propertyKey == LIST_CONTAINER_VIEW_DELEGATE) {
            ListContainerViewDelegate delegate = model.get(LIST_CONTAINER_VIEW_DELEGATE);
            if (delegate == null) {
                ntpCardsList.destroy();
            } else {
                ntpCardsList.renderAllListItems(delegate);
            }
        }
    }
}
