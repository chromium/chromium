// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.graphics.Bitmap;
import android.util.SparseArray;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo.FaviconProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

class ManualFillingComponentBridge {
    private final SparseArray<PropertyProvider<AccessorySheetData>> mProviders =
            new SparseArray<>();
    private final PropertyProvider<Action[]> mActionProvider =
            new PropertyProvider<>(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
    private final ManualFillingComponent mManualFillingComponent;
    private final ChromeActivity mActivity;
    private long mNativeView;

    private ManualFillingComponentBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        mManualFillingComponent = mActivity.getManualFillingComponent();
        mManualFillingComponent.registerActionProvider(mActionProvider);
    }

    PropertyProvider<AccessorySheetData> getOrCreateProvider(@AccessoryTabType int tabType) {
        PropertyProvider<AccessorySheetData> provider = mProviders.get(tabType);
        if (provider != null) return provider;
        provider = new PropertyProvider<>();
        mProviders.put(tabType, provider);
        mManualFillingComponent.registerSheetDataProvider(tabType, provider);
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
        getOrCreateProvider(accessorySheetData.getSheetType()).notifyObservers(accessorySheetData);
    }

    @CalledByNative
    private void onAutomaticGenerationStatusChanged(boolean available) {
        final Action[] generationAction;
        if (available) {
            // This is meant to suppress the warning that the short string is not used.
            // TODO(crbug.com/855581): Switch between strings based on whether they fit on the
            // screen or not.
            boolean useLongString = true;
            String caption = useLongString
                    ? mActivity.getString(R.string.password_generation_accessory_button)
                    : mActivity.getString(R.string.password_generation_accessory_button_short);
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
        mActionProvider.notifyObservers(generationAction);
    }

    @CalledByNative
    void showWhenKeyboardIsVisible() {
        mManualFillingComponent.showWhenKeyboardIsVisible();
    }

    @CalledByNative
    void hide() {
        mManualFillingComponent.hide();
    }

    @CalledByNative
    private void closeAccessorySheet() {
        mManualFillingComponent.closeAccessorySheet();
    }

    @CalledByNative
    private void swapSheetWithKeyboard() {
        mManualFillingComponent.swapSheetWithKeyboard();
    }

    @CalledByNative
    private void destroy() {
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
    private Object addUserInfoToAccessorySheetData(Object objAccessorySheetData, String origin) {
        UserInfo userInfo = new UserInfo(origin, this::fetchFavicon);
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

    private void fetchFavicon(String origin, @Px int desiredSize,
            Callback<FaviconProvider.FaviconResult> faviconCallback) {
        assert mNativeView != 0 : "Favicon was requested after the bridge was destroyed!";
        ManualFillingComponentBridgeJni.get().onFaviconRequested(mNativeView,
                ManualFillingComponentBridge.this, origin, desiredSize, faviconCallback);
    }

    @CalledByNative
    public static Object createFaviconResult(String origin, Bitmap favicon) {
        return new FaviconProvider.FaviconResult(origin, favicon);
    }

    @VisibleForTesting
    public static void cachePasswordSheetData(
            WebContents webContents, String[] userNames, String[] passwords) {
        ManualFillingComponentBridgeJni.get().cachePasswordSheetDataForTesting(
                webContents, userNames, passwords);
    }

    @VisibleForTesting
    public static void notifyFocusedFieldType(WebContents webContents, int focusedFieldType) {
        ManualFillingComponentBridgeJni.get().notifyFocusedFieldTypeForTesting(
                webContents, focusedFieldType);
    }

    @VisibleForTesting
    public static void signalAutoGenerationStatus(WebContents webContents, boolean available) {
        ManualFillingComponentBridgeJni.get().signalAutoGenerationStatusForTesting(
                webContents, available);
    }

    @NativeMethods
    interface Natives {
        void onFaviconRequested(long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller, String origin, int desiredSizeInPx,
                Callback<FaviconProvider.FaviconResult> faviconCallback);
        void onFillingTriggered(long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller, int tabType, UserInfoField userInfoField);
        void onOptionSelected(long nativeManualFillingViewAndroid,
                ManualFillingComponentBridge caller, int accessoryAction);
        void cachePasswordSheetDataForTesting(
                WebContents webContents, String[] userNames, String[] passwords);
        void notifyFocusedFieldTypeForTesting(WebContents webContents, int focusedFieldType);
        void signalAutoGenerationStatusForTesting(WebContents webContents, boolean available);
    }
}
