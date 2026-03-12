// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_ALL_NTP_CARDS_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_ALL_NTP_CARDS_SWITCH_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_MODULE_LIST_EDITABLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.view.View;
import android.widget.TextView;

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
        } else if (propertyKey == IS_ALL_NTP_CARDS_SWITCH_VISIBLE) {
            int visibility = model.get(IS_ALL_NTP_CARDS_SWITCH_VISIBLE) ? View.VISIBLE : View.GONE;
            if (allCardsSwitch != null) {
                allCardsSwitch.setVisibility(visibility);
            }
            TextView cardsSectionTitle = view.findViewById(R.id.cards_section_title);
            if (cardsSectionTitle != null) {
                cardsSectionTitle.setVisibility(visibility);
            }
        } else if (propertyKey == IS_ALL_NTP_CARDS_SWITCH_CHECKED) {
            if (allCardsSwitch != null) {
                allCardsSwitch.setChecked(model.get(IS_ALL_NTP_CARDS_SWITCH_CHECKED));
            }
        } else if (propertyKey == IS_MODULE_LIST_EDITABLE) {
            if (ntpCardsList != null) {
                ntpCardsList.setAllModuleSwitchesEnabled(model.get(IS_MODULE_LIST_EDITABLE));
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
