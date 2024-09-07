// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.IS_DEFAULT_A11Y_FOCUS_REQUESTED;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.ITEMS;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryToggleType;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.IbanInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.OptionToggle;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PasskeySection;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PlusAddressInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PromoCodeInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class contains the logic for the simple accessory sheets. Changes to its internal
 * {@link PropertyModel} are observed by a {@link PropertyModelChangeProcessor} and affect the
 * accessory sheet tab view.
 */
class AccessorySheetTabMediator implements Provider.Observer<AccessorySheetData> {
    private final PropertyModel mModel;
    private final @AccessoryTabType int mTabType;
    private final @Type int mUserInfoType;
    private final @AccessoryAction int mManageActionToRecord;
    private final ToggleChangeDelegate mToggleChangeDelegate;

    /**
     * Can be used to handle changes coming from the {@link OptionToggle}.
     *
     * <p>TODO(crbug.com/40702406): Remove the interface and the delegate field from this class and
     * handle the toggle changes via the PasswordAccessorySheetMediator.
     */
    public interface ToggleChangeDelegate {
        /**
         * Is triggered when the toggle state changes, either on tap or when it is
         * first initialized.
         *
         * @param enabled The new state of the toggle.
         */
        void onToggleChanged(boolean enabled);
    }

    @Override
    public void onItemAvailable(int typeId, AccessorySheetData accessorySheetData) {
        TraceEvent.begin("AccessorySheetTabMediator#onItemAvailable");
        mModel.get(ITEMS).set(splitIntoDataPieces(accessorySheetData));
        TraceEvent.end("AccessorySheetTabMediator#onItemAvailable");
    }

    AccessorySheetTabMediator(
            PropertyModel model,
            @AccessoryTabType int tabType,
            @Type int userInfoType,
            @AccessoryAction int manageActionToRecord,
            @Nullable ToggleChangeDelegate toggleChangeDelegate) {
        mModel = model;
        mTabType = tabType;
        mUserInfoType = userInfoType;
        mManageActionToRecord = manageActionToRecord;
        mToggleChangeDelegate = toggleChangeDelegate;
    }

    @CallSuper
    void onTabShown() {
        // This is a compromise: we log an impression, even if the user didn't scroll down far
        // enough to see it. If we moved it into the view layer (i.e. when the actual button is
        // created and shown), we could record multiple impressions of the user scrolls up and
        // down repeatedly.
        ManualFillingMetricsRecorder.recordActionImpression(mManageActionToRecord);
        recordToggleImpression();

        setDefaultA11yFocus();
    }

    void setDefaultA11yFocus() {
        if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            return;
        }

        // Reset the "requested" flag to make sure it takes effect in the binder.
        mModel.set(IS_DEFAULT_A11Y_FOCUS_REQUESTED, false);
        mModel.set(IS_DEFAULT_A11Y_FOCUS_REQUESTED, true);
    }

    private AccessorySheetDataPiece[] splitIntoDataPieces(AccessorySheetData accessorySheetData) {
        if (accessorySheetData == null) return new AccessorySheetDataPiece[0];

        List<AccessorySheetDataPiece> items = new ArrayList<>();
        if (accessorySheetData.getOptionToggle() != null) {
            items.add(createDataPieceForToggle(accessorySheetData.getOptionToggle()));
        }
        for (PromoCodeInfo promoCodeInfo : accessorySheetData.getPromoCodeInfoList()) {
            items.add(new AccessorySheetDataPiece(promoCodeInfo, Type.PROMO_CODE_INFO));
        }
        if (!accessorySheetData.getUserInfoTitle().isEmpty()) {
            items.add(
                    new AccessorySheetDataPiece(accessorySheetData.getUserInfoTitle(), Type.TITLE));
        }
        if (!accessorySheetData.getWarning().isEmpty()) {
            items.add(new AccessorySheetDataPiece(accessorySheetData.getWarning(), Type.WARNING));
        }
        if (accessorySheetData.getSheetType() == AccessoryTabType.ADDRESSES) {
            // Plus address section is displayed at the top for addresses tab.
            addPlusAddressSection(accessorySheetData, items);
        }
        for (PasskeySection passkey : accessorySheetData.getPasskeySectionList()) {
            items.add(new AccessorySheetDataPiece(passkey, Type.PASSKEY_SECTION));
        }
        for (UserInfo userInfo : accessorySheetData.getUserInfoList()) {
            items.add(new AccessorySheetDataPiece(userInfo, mUserInfoType));
        }
        if (accessorySheetData.getSheetType() == AccessoryTabType.PASSWORDS) {
            // Plus address section is displayed at the bottom for passwords tab.
            addPlusAddressSection(accessorySheetData, items);
        }
        for (IbanInfo ibanInfo : accessorySheetData.getIbanInfoList()) {
            items.add(new AccessorySheetDataPiece(ibanInfo, Type.IBAN_INFO));
        }
        for (FooterCommand command : accessorySheetData.getFooterCommands()) {
            items.add(new AccessorySheetDataPiece(command, Type.FOOTER_COMMAND));
        }

        return items.toArray(new AccessorySheetDataPiece[0]);
    }

    private void addPlusAddressSection(
            AccessorySheetData data, List<AccessorySheetDataPiece> items) {
        if (!data.getPlusAddressSectionTitle().isEmpty()) {
            items.add(new AccessorySheetDataPiece(data.getPlusAddressSectionTitle(), Type.TITLE));
        }
        for (PlusAddressInfo plusAddress : data.getPlusAddressInfoList()) {
            items.add(new AccessorySheetDataPiece(plusAddress, Type.PLUS_ADDRESS_SECTION));
        }
    }

    private AccessorySheetDataPiece createDataPieceForToggle(OptionToggle toggle) {
        assert mToggleChangeDelegate != null
                : "Toggles added in an accessory sheet should have a" + "toggle change delegate.";
        // Make sure the delegate knows the initial state of the toggle.
        mToggleChangeDelegate.onToggleChanged(toggle.isEnabled());
        OptionToggle toggleWithAddedCallback =
                new OptionToggle(
                        toggle.getDisplayText(),
                        toggle.isEnabled(),
                        toggle.getActionType(),
                        enabled -> {
                            ManualFillingMetricsRecorder.recordToggleClicked(
                                    getRecordingTypeForToggle(toggle));
                            updateOptionToggleEnabled();
                            mToggleChangeDelegate.onToggleChanged(enabled);
                            toggle.getCallback().onResult(enabled);
                        });
        return new AccessorySheetDataPiece(toggleWithAddedCallback, Type.OPTION_TOGGLE);
    }

    private void updateOptionToggleEnabled() {
        for (int i = 0; i < mModel.get(ITEMS).size(); ++i) {
            AccessorySheetDataPiece data = mModel.get(ITEMS).get(i);
            if (AccessorySheetDataPiece.getType(data) == Type.OPTION_TOGGLE) {
                OptionToggle toggle = (OptionToggle) data.getDataPiece();
                OptionToggle updatedToggle =
                        new OptionToggle(
                                toggle.getDisplayText(),
                                !toggle.isEnabled(),
                                toggle.getActionType(),
                                toggle.getCallback());
                mModel.get(ITEMS)
                        .update(i, new AccessorySheetDataPiece(updatedToggle, Type.OPTION_TOGGLE));
                break;
            }
        }
    }

    private void recordToggleImpression() {
        for (int i = 0; i < mModel.get(ITEMS).size(); ++i) {
            AccessorySheetDataPiece data = mModel.get(ITEMS).get(i);
            if (AccessorySheetDataPiece.getType(data) == Type.OPTION_TOGGLE) {
                OptionToggle toggle = (OptionToggle) data.getDataPiece();
                ManualFillingMetricsRecorder.recordToggleImpression(
                        getRecordingTypeForToggle(toggle));
            }
        }
    }

    private @AccessoryToggleType int getRecordingTypeForToggle(OptionToggle toggle) {
        if (toggle.getActionType() == AccessoryAction.TOGGLE_SAVE_PASSWORDS) {
            return toggle.isEnabled()
                    ? AccessoryToggleType.SAVE_PASSWORDS_TOGGLE_ON
                    : AccessoryToggleType.SAVE_PASSWORDS_TOGGLE_OFF;
        }
        assert false
                : "Recording type for toggle of type " + toggle.getActionType() + "is not known.";
        return AccessoryToggleType.COUNT;
    }
}
