// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.password_manager.ConfirmationDialogHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Handles requests to the manual UI for filling passwords, payments and other user data. Ideally,
 * the caller has no access to Keyboard accessory or sheet and is only interacting with this
 * component. For that, it facilitates the communication between {@link
 * KeyboardAccessoryCoordinator} and {@link AccessorySheetCoordinator} to add and trigger surfaces
 * that may assist users while filling fields.
 */
class ManualFillingCoordinator implements ManualFillingComponent {
    private final ManualFillingMediator mMediator = new ManualFillingMediator();
    private ObserverList<Observer> mObserverList = new ObserverList<>();

    public ManualFillingCoordinator() {}

    @Override
    public void initialize(
            WindowAndroid windowAndroid,
            Profile profile,
            BottomSheetController sheetController,
            BooleanSupplier isContextualSearchOpened,
            SoftKeyboardDelegate keyboardDelegate,
            BackPressManager backPressManager,
            Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            AsyncViewStub sheetStub,
            AsyncViewStub barStub) {
        Context context = windowAndroid.getContext().get();
        if (barStub == null || sheetStub == null || context == null) {
            return; // The manual filling isn't needed.
        }
        // TODO(crbug.com/40269514): Initialize in the xml resources file.
        barStub.setLayoutResource(R.layout.keyboard_accessory);
        sheetStub.setLayoutResource(R.layout.keyboard_accessory_sheet);
        barStub.setShouldInflateOnBackgroundThread(true);
        sheetStub.setShouldInflateOnBackgroundThread(true);
        initialize(
                windowAndroid,
                new KeyboardAccessoryCoordinator(profile, mMediator, mMediator, barStub),
                new AccessorySheetCoordinator(sheetStub, mMediator),
                sheetController,
                isContextualSearchOpened,
                backPressManager,
                edgeToEdgeControllerSupplier,
                keyboardDelegate,
                new ConfirmationDialogHelper(context));
    }

    @VisibleForTesting
    void initialize(
            WindowAndroid windowAndroid,
            KeyboardAccessoryCoordinator accessoryBar,
            AccessorySheetCoordinator accessorySheet,
            BottomSheetController sheetController,
            BooleanSupplier isContextualSearchOpened,
            BackPressManager backPressManager,
            Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            SoftKeyboardDelegate keyboardDelegate,
            ConfirmationDialogHelper confirmationHelper) {
        mMediator.initialize(
                accessoryBar,
                accessorySheet,
                windowAndroid,
                sheetController,
                isContextualSearchOpened,
                backPressManager,
                edgeToEdgeControllerSupplier,
                keyboardDelegate,
                confirmationHelper);
    }

    @Override
    public void destroy() {
        for (Observer observer : mObserverList) observer.onDestroy();
        mMediator.destroy();
    }

    @Override
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }

    @Override
    public void dismiss() {
        mMediator.dismiss();
    }

    @Override
    public void notifyPopupAvailable(DropdownPopupWindow popup) {
        mMediator.notifyPopupOpened(popup);
    }

    @Override
    public void closeAccessorySheet() {
        mMediator.onCloseAccessorySheet();
    }

    @Override
    public void swapSheetWithKeyboard() {
        mMediator.swapSheetWithKeyboard();
    }

    @Override
    public void registerActionProvider(
            WebContents webContents,
            PropertyProvider<KeyboardAccessoryData.Action[]> actionProvider) {
        mMediator.registerActionProvider(webContents, actionProvider);
    }

    @Override
    public void registerSheetDataProvider(
            WebContents webContents,
            @AccessoryTabType int sheetType,
            PropertyProvider<KeyboardAccessoryData.AccessorySheetData> sheetDataProvider) {
        mMediator.registerSheetDataProvider(webContents, sheetType, sheetDataProvider);
    }

    @Override
    public void registerSheetUpdateDelegate(
            WebContents webContents, UpdateAccessorySheetDelegate delegate) {
        mMediator.registerSheetUpdateDelegate(webContents, delegate);
    }

    @Override
    public void registerAutofillProvider(
            PropertyProvider<List<AutofillSuggestion>> autofillProvider,
            AutofillDelegate delegate) {
        mMediator.registerAutofillProvider(autofillProvider, delegate);
    }

    @Override
    public void show(boolean waitForKeyboard) {
        mMediator.show(waitForKeyboard);
    }

    @Override
    public void hide() {
        mMediator.hide();
    }

    @Override
    public void showAccessorySheetTab(@AccessoryTabType int tabType) {
        mMediator.showAccessorySheetTab(tabType);
    }

    @Override
    public void onResume() {
        mMediator.resume();
    }

    @Override
    public void onPause() {
        mMediator.pause();
    }

    @Override
    public boolean isFillingViewShown(View view) {
        return mMediator.isFillingViewShown(view);
    }

    @Override
    public ObservableSupplier<Integer> getBottomInsetSupplier() {
        return mMediator.getBottomInsetSupplier();
    }

    @Override
    public boolean addObserver(Observer observer) {
        return mObserverList.addObserver(observer);
    }

    @Override
    public boolean removeObserver(Observer observer) {
        return mObserverList.addObserver(observer);
    }

    @Override
    public void confirmOperation(
            String title, String message, Runnable confirmedCallback, Runnable declinedCallback) {
        mMediator.confirmOperation(title, message, confirmedCallback, declinedCallback);
    }

    ManualFillingMediator getMediatorForTesting() {
        return mMediator;
    }

    @Override
    public int getKeyboardExtensionHeight() {
        return mMediator != null ? mMediator.getKeyboardExtensionHeight() : 0;
    }

    @Override
    public ObservableSupplier<AccessorySheetVisualStateProvider>
            getAccessorySheetVisualStateProvider() {
        return mMediator.getAccessorySheetVisualStateProvider();
    }

    @Override
    public void forceShowForTesting() {
        mMediator.show(true);
    }
}
