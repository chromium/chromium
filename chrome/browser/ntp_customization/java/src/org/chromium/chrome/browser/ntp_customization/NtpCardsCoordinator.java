// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_BACK_PRESS_HANDLER;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the NTP cards bottom sheet. */
public class NtpCardsCoordinator {
    public NtpCardsCoordinator(
            Context context, BottomSheetDelegate delegate, PropertyModel propertyModel) {
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_ntp_cards_bottom_sheet, null, false);
        delegate.registerBottomSheetLayout(NTP_CARDS, view);

        // TODO(crbug.com/397439004): NtpCardsCoordinator creates it own property model instead of
        // sharing it with other coordinators
        propertyModel.set(
                NTP_CARDS_BACK_PRESS_HANDLER, v -> delegate.backPressOnCurrentBottomSheet());
    }
}
