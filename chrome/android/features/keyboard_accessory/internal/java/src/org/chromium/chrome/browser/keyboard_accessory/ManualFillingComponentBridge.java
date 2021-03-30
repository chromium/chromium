// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.app.Activity;
import android.util.SparseArray;
import androidx.annotation.VisibleForTesting;
import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.OptionToggle;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

class ManualFillingComponentBridge {
    private final SparseArray<PropertyProvider<AccessorySheetData>> mProviders =
            new SparseArray<>();
    private PropertyProvider<Action[]> mActionProvider;
    private final WindowAndroid mWindowAndroid;
    private long mNativeView;
    private final ManualFillingComponent.Observer mDestructionObserver = this::onComponentDestroyed;

    private ManualFillingComponentBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        mWindowAndroid = windowAndroid;
    }

    PropertyProvider<AccessorySheetData> getOrCreateProvider(@AccessoryTabType int tabType) {
        PropertyProvider<AccessorySheetData> provider = mProviders.get(tabType);
        if (provider != null) return provider;
        if (getManualFillingComponent() == null) return null;
        provider = new PropertyProvider<>();
        mProviders.put(tabType, provider);
        getManualFillingComponent().registerSheetDataProvider(tabType, provider);
        return provider;
    }

    @CalledByNative
    private static ManualFillingComponentBridge create(
            long nativeView, WindowAndroid windowAndroid) {
        return new ManualFillingComponentBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void onItemsAvailable(Object objAccessorySheetData) {
        AccessorySheetData accessorySheetData = (AccessorySheetData) objAccessorySheetData;
        PropertyProvider<AccessorySheetData> provider =
                getOrCreateProvider(accessorySheetData.getSheetType());
        if (provider != null) provider.notifyObservers(accessorySheetData);
    }

    @CalledByNative
    private void onAutomaticGenerationStatusChanged(boolean available) {
        final Action[] generationAction;
        final Activity activity = mWindowAndroid.getActivity().get();
        if (available && activity != null) {
            // This is meant to suppress the warning that the short string is not used.
            // TODO(crbug.com/855581): Switch between strings based on whether they fit on the
            // screen or not.
            boolean useLongString = true;
            String caption = useLongString
                    ? activity.getString(R.string.password_generation_accessory_button)
                    : activity.getString(R.string.password_generation_accessory_button_short);
            generationAction = new Action[] {
                    new Action(caption, AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, (action) -> {
                        assert mNativeView
                                != 0
                            : "Controller has been destroyed but the bridge wasn't cleaned up!";
                        ManualFillingMetricsRecorder.recordActionSelected(
                                AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
                        ManualFillingComponentBridgeJni.get().onOptionSelected(mNativeView,
                                ManualFillingComponentBridge.this,
                                AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
                    })};
        } else {
            generationAction = new Action[0];
        }
        if (mActionProvider == null && getManualFillingComponent() != null) {
            mActionProvider = new PropertyProvider<>(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
            getManualFillingComponent().registerActionProvider(mActionProvider);
        }
        if (mActionProvider != null) mActionProvider.notifyObservers(generationAction);
    }

    @CalledByNative
    void showWhenKeyboardIsVisible() {
        if (getManualFillingComponent() != null) {
            getManualFillingComponent().showWhenKeyboardIsVisible();
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
    private static Object createAccessorySheetData(
            @AccessoryTabType int type, String title, String warning) {
        return new AccessorySheetData(type, title, warning);
    }

    @CalledByNative
    private void addOptionToggleToAccessorySheetData(Object objAccessorySheetData,
            String displayText, boolean enabled, @AccessoryAction int accessoryAction) {
        ((AccessorySheetData) objAccessorySheetData)
                .setOptionToggle(new OptionToggle(displayText, enabled, accessoryAction, on -> {
                    assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                    ManualFillingComponentBridgeJni.get().onToggleChanged(
                            mNativeView, ManualFillingComponentBridge.this, accessoryAction, on);
                }));
    }

    @CalledByNative
    private Object addUserInfoToAccessorySheetData(
            Object objAccessorySheetData, String origin, boolean isPslMatch) {
        UserInfo userInfo = new UserInfo(origin, isPslMatch);
        ((AccessorySheetData) objAccessorySheetData).getUserInfoList().add(userInfo);
        return userInfo;
    }

    @CalledByNative
    private void addFieldToUserInfo(Object objUserInfo, @AccessoryTabType int sheetType,
            String displayText, String a11yDescription, String guid, boolean isObfuscated,
            boolean selectable) {
        Callback<UserInfoField> callback = null;
        if (selectable) {
            callback = (field) -> {
                assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                ManualFillingMetricsRecorder.recordSuggestionSelected(
                        sheetType, field.isObfuscated());
                ManualFillingComponentBridgeJni.get().onFillingTriggered(
                        mNativeView, ManualFillingComponentBridge.this, sheetType, field);
            };
        }
        ((UserInfo) objUserInfo)
                .getFields()
                .add(new UserInfoField(displayText, a11yDescription, guid, isObfuscated, callback));
    }

    @CalledByNative
    private void addFooterCommandToAccessorySheetData(
            Object objAccessorySheetData, String displayText, int accessoryAction) {
        ((AccessorySheetData) objAccessorySheetData)
                .getFooterCommands()
                .add(new FooterCommand(displayText, (footerCommand) -> {
                    assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                    ManualFillingComponentBridgeJni.get().onOptionSelected(
                            mNativeView, ManualFillingComponentBridge.this, accessoryAction);
                }));
    }

    @VisibleForTesting
    public static void cachePasswordSheetData(WebContents webContents, String[] userNames,
            String[] passwords, boolean originDenylisted) {
        ManualFillingComponentBridgeJni.get().cachePasswordSheetDataForTesting(
                webContents, userNames, passwords, originDenylisted);
    }

    @VisibleForTesting
    public static void notifyFocusedFieldType(
            WebContents webContents, long focusedFieldId, int focusedFieldType) {
        ManualFillingComponentBridgeJni.get().notifyFocusedFieldTypeForTesting(
                webContents, focusedFieldId, focusedFieldType);
    }

    @VisibleForTesting
    public static void signalAutoGenerationStatus(WebContents webContents, boolean available) {
        ManualFillingComponentBridgeJni.get().signalAutoGenerationStatusForTesting(
                webContents, available);
    }

    @VisibleForTesting
    public static void disableServerPredictionsForTesting() {
        ManualFillingComponentBridgeJni.get().disableServerPredictionsForTesting();
    }

    private ManualFillingComponent getManualFillingComponent() {
        ChromeActivity activity = (ChromeActivity) mWindowAndroid.getActivity().get();
        if (activity == null) return null; // Has the activity died since it was last checked?
        activity.getManualFillingComponent().addObserver(mDestructionObserver);
        return activity.getManualFillingComponent();
    }

    private void onComponentDestroyed() {
        if (mNativeView != 0) {
            ManualFillingComponentBridgeJni.get().onViewDestroyed(
                    mNativeView, ManualFillingComponentBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        void onFillingTriggered(long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller, int tabType, UserInfoField userInfoField);
        void onOptionSelected(long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller, int accessoryAction);
        void onToggleChanged(long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller, int accessoryAction, boolean enabled);
        void onViewDestroyed(
                long nativeManualFillingViewAndroid, ManualFillingComponentBridge caller);
        void cachePasswordSheetDataForTesting(WebContents webContents, String[] userNames,
                String[] passwords, boolean originDenylisted);
        void notifyFocusedFieldTypeForTesting(
                WebContents webContents, long focusedFieldId, int focusedFieldType);
        void signalAutoGenerationStatusForTesting(WebContents webContents, boolean available);
        void disableServerPredictionsForTesting();
    }
}
