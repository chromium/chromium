// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.util.SparseArray;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.IbanInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.OptionToggle;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PasskeySection;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PlusAddressInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PromoCodeInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;

class ManualFillingComponentBridge {
    private final SparseArray<PropertyProvider<AccessorySheetData>> mProviders =
            new SparseArray<>();
    private HashMap<Integer, PropertyProvider<Action[]>> mActionProviders = new HashMap<>();
    private final WindowAndroid mWindowAndroid;
    private final WebContents mWebContents;
    private long mNativeView;
    private final ManualFillingComponent.Observer mDestructionObserver = this::onComponentDestroyed;

    private ManualFillingComponentBridge(
            long nativeView, WindowAndroid windowAndroid, WebContents webContents) {
        mNativeView = nativeView;
        mWindowAndroid = windowAndroid;
        mWebContents = webContents;
    }

    PropertyProvider<AccessorySheetData> getOrCreateProvider(@AccessoryTabType int tabType) {
        PropertyProvider<AccessorySheetData> provider = mProviders.get(tabType);
        if (provider != null) return provider;
        if (getManualFillingComponent() == null) return null;
        if (mWebContents.isDestroyed()) return null;
        if (mProviders.size() == 0) { // True iff the component is available for the first time.
            getManualFillingComponent()
                    .registerSheetUpdateDelegate(mWebContents, this::requestSheet);
        }
        provider = new PropertyProvider<>();
        mProviders.put(tabType, provider);
        getManualFillingComponent().registerSheetDataProvider(mWebContents, tabType, provider);
        return provider;
    }

    @CalledByNative
    private static ManualFillingComponentBridge create(
            long nativeView, WindowAndroid windowAndroid, WebContents webContents) {
        return new ManualFillingComponentBridge(nativeView, windowAndroid, webContents);
    }

    @CalledByNative
    private void onItemsAvailable(AccessorySheetData accessorySheetData) {
        assertOnUiThread();
        PropertyProvider<AccessorySheetData> provider =
                getOrCreateProvider(accessorySheetData.getSheetType());
        if (provider != null) provider.notifyObservers(accessorySheetData);
    }

    @CalledByNative
    private void onAccessoryActionAvailabilityChanged(
            boolean available, @AccessoryAction int actionType) {
        createOrClearAction(available, actionType);
    }

    @CalledByNative
    void show(boolean waitForKeyboard) {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().show(waitForKeyboard);
        }
    }

    @CalledByNative
    void hide() {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().hide();
        }
    }

    @CalledByNative
    private void closeAccessorySheet() {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().closeAccessorySheet();
        }
    }

    @CalledByNative
    private void swapSheetWithKeyboard() {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().swapSheetWithKeyboard();
        }
    }

    @CalledByNative
    private void destroy() {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().removeObserver(mDestructionObserver);
        }
        for (int i = 0; i < mProviders.size(); ++i) {
            mProviders.valueAt(i).notifyObservers(null);
        }
        mNativeView = 0;
    }

    @CalledByNative
    private static AccessorySheetData createAccessorySheetData(
            @AccessoryTabType int type,
            @JniType("std::u16string") String userInfoTitle,
            @JniType("std::u16string") String plusAddressTitle,
            @JniType("std::u16string") String warning) {
        return new AccessorySheetData(type, userInfoTitle, plusAddressTitle, warning);
    }

    @CalledByNative
    private void showAccessorySheetTab(int tabType) {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().showAccessorySheetTab(tabType);
        }
    }

    @CalledByNative
    private void addOptionToggleToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @JniType("std::u16string") String displayText,
            boolean enabled,
            @AccessoryAction int accessoryAction) {
        accessorySheetData.setOptionToggle(
                new OptionToggle(
                        displayText,
                        enabled,
                        accessoryAction,
                        on -> {
                            assert mNativeView != 0
                                    : "Controller was destroyed but the bridge wasn't!";
                            ManualFillingComponentBridgeJni.get()
                                    .onToggleChanged(
                                            mNativeView,
                                            ManualFillingComponentBridge.this,
                                            accessoryAction,
                                            on);
                        }));
    }

    @CalledByNative
    private UserInfo addUserInfoToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @JniType("std::string") String origin,
            boolean isExactMatch,
            GURL iconUrl) {
        UserInfo userInfo = new UserInfo(origin, isExactMatch, iconUrl);
        accessorySheetData.getUserInfoList().add(userInfo);
        return userInfo;
    }

    @CalledByNative
    private void addFieldToUserInfo(
            UserInfo userInfo,
            @AccessoryTabType int sheetType,
            @JniType("std::u16string") String displayText,
            @JniType("std::u16string") String textToFill,
            @JniType("std::u16string") String a11yDescription,
            @JniType("std::string") String guid,
            int iconId,
            boolean isObfuscated,
            boolean selectable) {
        Callback<UserInfoField> callback = null;
        if (selectable) {
            callback =
                    (field) -> {
                        assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                        ManualFillingMetricsRecorder.recordSuggestionSelected(
                                sheetType, field.isObfuscated());
                        ManualFillingComponentBridgeJni.get()
                                .onFillingTriggered(
                                        mNativeView,
                                        ManualFillingComponentBridge.this,
                                        sheetType,
                                        field);
                    };
        }
        userInfo.getFields()
                .add(
                        new UserInfoField.Builder()
                                .setDisplayText(displayText)
                                .setTextToFill(textToFill)
                                .setA11yDescription(a11yDescription)
                                .setId(guid)
                                .setIconId(iconId)
                                .setIsObfuscated(isObfuscated)
                                .setCallback(callback)
                                .build());
    }

    @CalledByNative
    private void addPlusAddressInfoToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @AccessoryTabType int sheetType,
            @JniType("std::string") String origin,
            @JniType("std::u16string") String plusAddress) {
        Callback<UserInfoField> callback =
                (field) -> {
                    assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                    // TODO: crbug.com/327838324 - Record that the suggestion was accepted.
                    ManualFillingComponentBridgeJni.get()
                            .onFillingTriggered(mNativeView, this, sheetType, field);
                };
        UserInfoField field =
                new UserInfoField.Builder()
                        .setDisplayText(plusAddress)
                        .setTextToFill(plusAddress)
                        .setA11yDescription(plusAddress)
                        .setId("")
                        .setIsObfuscated(false)
                        .setCallback(callback)
                        .build();

        accessorySheetData.getPlusAddressInfoList().add(new PlusAddressInfo(origin, field));
    }

    @CalledByNative
    private void addPasskeySectionToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @AccessoryTabType int sheetType,
            @JniType("std::string") String displayName,
            @JniType("std::vector<uint8_t>") byte[] passkeyId) {
        accessorySheetData
                .getPasskeySectionList()
                .add(
                        new PasskeySection(
                                displayName,
                                () -> {
                                    assert mNativeView != 0
                                            : "Controller was destroyed but the bridge wasn't!";
                                    ManualFillingComponentBridgeJni.get()
                                            .onPasskeySelected(
                                                    mNativeView,
                                                    ManualFillingComponentBridge.this,
                                                    sheetType,
                                                    passkeyId);
                                }));
    }

    @CalledByNative
    private void addPromoCodeInfoToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @AccessoryTabType int sheetType,
            @JniType("std::u16string") String displayText,
            @JniType("std::u16string") String textToFill,
            @JniType("std::u16string") String a11yDescription,
            @JniType("std::string") String guid,
            boolean isObfuscated,
            @JniType("std::u16string") String detailsText) {
        PromoCodeInfo promoCodeInfo = new PromoCodeInfo();
        accessorySheetData.getPromoCodeInfoList().add(promoCodeInfo);

        Callback<UserInfoField> callback = null;
        callback =
                (field) -> {
                    assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                    ManualFillingMetricsRecorder.recordSuggestionSelected(
                            sheetType, field.isObfuscated());
                    ManualFillingComponentBridgeJni.get()
                            .onFillingTriggered(
                                    mNativeView,
                                    ManualFillingComponentBridge.this,
                                    sheetType,
                                    field);
                };
        ((PromoCodeInfo) promoCodeInfo)
                .setPromoCode(
                        new UserInfoField.Builder()
                                .setDisplayText(displayText)
                                .setTextToFill(textToFill)
                                .setA11yDescription(a11yDescription)
                                .setId(guid)
                                .setIsObfuscated(isObfuscated)
                                .setCallback(callback)
                                .build());
        ((PromoCodeInfo) promoCodeInfo).setDetailsText(detailsText);
    }

    @CalledByNative
    private void addIbanInfoToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @AccessoryTabType int sheetType,
            @JniType("std::string") String guid,
            @JniType("std::u16string") String value,
            @JniType("std::u16string") String textToFill) {
        IbanInfo ibanInfo = new IbanInfo();
        accessorySheetData.getIbanInfoList().add(ibanInfo);

        Callback<UserInfoField> callback =
                (field) -> {
                    assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                    ManualFillingComponentBridgeJni.get()
                            .onFillingTriggered(mNativeView, this, sheetType, field);
                };
        ((IbanInfo) ibanInfo)
                .setValue(
                        new UserInfoField.Builder()
                                .setDisplayText(value)
                                .setTextToFill(textToFill)
                                .setA11yDescription(value)
                                .setIsObfuscated(true)
                                .setId(guid)
                                .setCallback(callback)
                                .build());
    }

    @CalledByNative
    private void addFooterCommandToAccessorySheetData(
            AccessorySheetData accessorySheetData,
            @JniType("std::u16string") String displayText,
            int accessoryAction) {
        accessorySheetData
                .getFooterCommands()
                .add(
                        new FooterCommand(
                                displayText,
                                (footerCommand) -> {
                                    assert mNativeView != 0
                                            : "Controller was destroyed but the bridge wasn't!";
                                    ManualFillingComponentBridgeJni.get()
                                            .onOptionSelected(
                                                    mNativeView,
                                                    ManualFillingComponentBridge.this,
                                                    accessoryAction);
                                }));
    }

    @VisibleForTesting
    public static void cachePasswordSheetData(
            WebContents webContents,
            String[] userNames,
            String[] passwords,
            boolean originDenylisted) {
        ManualFillingComponentBridgeJni.get()
                .cachePasswordSheetDataForTesting(
                        webContents, userNames, passwords, originDenylisted);
    }

    @VisibleForTesting
    public static void notifyFocusedFieldType(
            WebContents webContents, long focusedFieldId, int focusedFieldType) {
        ManualFillingComponentBridgeJni.get()
                .notifyFocusedFieldTypeForTesting(webContents, focusedFieldId, focusedFieldType);
    }

    @VisibleForTesting
    public static void signalAutoGenerationStatus(WebContents webContents, boolean available) {
        ManualFillingComponentBridgeJni.get()
                .signalAutoGenerationStatusForTesting(webContents, available);
    }

    public static void disableServerPredictionsForTesting() {
        ManualFillingComponentBridgeJni.get().disableServerPredictionsForTesting();
    }

    private ManualFillingComponent getManualFillingComponent() {
        Supplier<ManualFillingComponent> manualFillingComponentSupplier =
                ManualFillingComponentSupplier.from(mWindowAndroid);
        if (manualFillingComponentSupplier == null) return null;

        ManualFillingComponent component = manualFillingComponentSupplier.get();
        if (component != null) {
            component.addObserver(mDestructionObserver);
        }

        return component;
    }

    private void onComponentDestroyed() {
        if (mNativeView == 0) return; // Component was destroyed already.
        ManualFillingComponentBridgeJni.get()
                .onViewDestroyed(mNativeView, ManualFillingComponentBridge.this);
    }

    private void requestSheet(int sheetType) {
        if (mNativeView == 0) return; // Component was destroyed already.
        ManualFillingComponentBridgeJni.get()
                .requestAccessorySheet(mNativeView, ManualFillingComponentBridge.this, sheetType);
    }

    private void createOrClearAction(boolean available, @AccessoryAction int actionType) {
        if (getManualFillingComponent() == null) return; // Actions are not displayed.
        final Action[] actions = available ? createSingleAction(actionType) : new Action[0];
        getOrCreateActionProvider(actionType).notifyObservers(actions);
    }

    private Action[] createSingleAction(@AccessoryAction int actionType) {
        return new Action[] {new Action(actionType, this::onActionSelected)};
    }

    private PropertyProvider<Action[]> getOrCreateActionProvider(@AccessoryAction int actionType) {
        assert getManualFillingComponent() != null
                : "Bridge has been destroyed but the bridge wasn't cleaned-up!";
        if (mActionProviders.containsKey(actionType)) {
            return mActionProviders.get(actionType);
        }
        PropertyProvider<Action[]> actionProvider = new PropertyProvider<>(actionType);
        mActionProviders.put(actionType, actionProvider);
        getManualFillingComponent().registerActionProvider(mWebContents, actionProvider);
        return actionProvider;
    }

    private void onActionSelected(Action action) {
        if (mNativeView == 0) return; // Component was destroyed already.
        ManualFillingMetricsRecorder.recordActionSelected(action.getActionType());
        ManualFillingComponentBridgeJni.get()
                .onOptionSelected(
                        mNativeView, ManualFillingComponentBridge.this, action.getActionType());
    }

    @NativeMethods
    interface Natives {
        void onFillingTriggered(
                long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller,
                int tabType,
                UserInfoField userInfoField);

        void onPasskeySelected(
                long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller,
                int tabType,
                @JniType("std::vector<uint8_t>") byte[] passkeyId);

        void onOptionSelected(
                long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller,
                int accessoryAction);

        void onToggleChanged(
                long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller,
                int accessoryAction,
                boolean enabled);

        void onViewDestroyed(
                long nativeManualFillingViewAndroid, ManualFillingComponentBridge caller);

        void requestAccessorySheet(
                long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller,
                int sheetType);

        void cachePasswordSheetDataForTesting(
                WebContents webContents,
                @JniType("std::vector<std::string>") String[] userNames,
                @JniType("std::vector<std::string>") String[] passwords,
                boolean originDenylisted);

        void notifyFocusedFieldTypeForTesting(
                WebContents webContents, long focusedFieldId, int focusedFieldType);

        void signalAutoGenerationStatusForTesting(WebContents webContents, boolean available);

        void disableServerPredictionsForTesting();
    }
}
