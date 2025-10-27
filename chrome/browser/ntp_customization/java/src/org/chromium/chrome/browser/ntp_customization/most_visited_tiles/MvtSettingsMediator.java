// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_MVT_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.SET_MVT_SWITCH_CONTENT_DESCRIPTION_RES_ID;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Most Visited Tiles settings bottom sheet. */
@NullMarked
public class MvtSettingsMediator {
    private final PropertyModel mBottomSheetPropertyModel;
    private final BottomSheetDelegate mBottomSheetDelegate;

    public MvtSettingsMediator(
            PropertyModel bottomSheetPropertyModel, BottomSheetDelegate delegate) {
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mBottomSheetDelegate = delegate;

        // Hides the back button when the mvt settings bottom sheet is displayed standalone.
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER,
                delegate.shouldShowAlone()
                        ? null
                        : v -> mBottomSheetDelegate.backPressOnCurrentBottomSheet());
        mBottomSheetPropertyModel.set(IS_MVT_SWITCH_CHECKED, isMvtTurnedOn());
        mBottomSheetPropertyModel.set(
                MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                (compoundButton, isChecked) -> onMvtSwitchToggled(isChecked));
        mBottomSheetPropertyModel.set(
                SET_MVT_SWITCH_CONTENT_DESCRIPTION_RES_ID,
                R.string.ntp_customization_turn_on_mvt_settings);
    }

    /** Returns whether the most visited tiles section are turned on and visible to the user. */
    boolean isMvtTurnedOn() {
        return NtpCustomizationConfigManager.getInstance().getPrefIsMvtToggleOn();
    }

    @VisibleForTesting
    void onMvtSwitchToggled(boolean isEnabled) {
        NtpCustomizationMetricsUtils.recordMvtToggledInBottomSheet(isEnabled);
        NtpCustomizationConfigManager.getInstance().setPrefIsMvtToggleOn(isEnabled);
    }

    void destroy() {
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mBottomSheetPropertyModel.set(MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER, null);
    }
}
