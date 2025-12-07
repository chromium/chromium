// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static android.view.View.VISIBLE;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetListContainerViewBinder;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.function.Supplier;

/** Coordinator for the NTP cards bottom sheet. */
@NullMarked
public class NtpCardsCoordinator {
    private final View mView;
    private NtpCardsMediator mMediator;

    public NtpCardsCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            Supplier<@Nullable Profile> profileSupplier) {
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_ntp_cards_bottom_sheet, null, false);
        mView = view;
        // TODO(crbug.com/458409311): Change these views to always be visible in the XML.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)) {
            view.findViewById(R.id.cards_switch_button).setVisibility(VISIBLE);
            view.findViewById(R.id.cards_section_title).setVisibility(VISIBLE);
        }

        delegate.registerBottomSheetLayout(NTP_CARDS, view);

        // The containerPropertyModel is responsible for managing a BottomSheetDelegate which
        // provides list content and event handlers to the list container view.
        PropertyModel containerPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.LIST_CONTAINER_KEYS);
        PropertyModelChangeProcessor.create(
                containerPropertyModel,
                view.findViewById(R.id.ntp_cards_container),
                BottomSheetListContainerViewBinder::bind);

        // The bottomSheetPropertyModel is responsible for managing the back press handler of the
        // back button in the bottom sheet.
        PropertyModel bottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        PropertyModelChangeProcessor.create(
                bottomSheetPropertyModel, view, BottomSheetViewBinder::bind);

        // The ntpCardsPropertyModel is responsible for managing the "all NTP cards" toggle, which
        // disables/enables the clickability of the other toggles and hides the other cards if
        // un-checked.
        PropertyModel ntpCardsPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.NTP_CARD_SETTINGS_KEYS);
        PropertyModelChangeProcessor.create(
                ntpCardsPropertyModel, view, NtpCardsBottomSheetViewBinder::bind);

        mMediator =
                new NtpCardsMediator(
                        containerPropertyModel,
                        bottomSheetPropertyModel,
                        ntpCardsPropertyModel,
                        delegate,
                        profileSupplier);
    }

    public void destroy() {
        mMediator.destroy();
    }

    /** Reacts to a configuration change of the "all NTP cards" toggle. */
    public void onAllCardsConfigChanged(boolean isEnabled) {
        mMediator.onAllCardsConfigChanged(isEnabled);
    }

    NtpCardsMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(NtpCardsMediator mediator) {
        mMediator = mediator;
    }

    View getViewForTesting() {
        return mView;
    }
}
