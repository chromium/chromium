// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.view.View;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles requests to the manual UI for filling passwords, payments and other user data. Ideally,
 * the caller has no access to Keyboard accessory or sheet and is only interacting with this
 * component.
 * For that, it facilitates the communication between {@link KeyboardAccessoryCoordinator} and
 * {@link AccessorySheetCoordinator} to add and trigger surfaces that may assist users while filling
 * fields.
 */
class ManualFillingCoordinator implements ManualFillingComponent {
    private final ManualFillingMediator mMediator = new ManualFillingMediator();

    public ManualFillingCoordinator() {}

    @Override
    public void initialize(WindowAndroid windowAndroid, ViewStub barStub, ViewStub sheetStub) {
        if (barStub == null || sheetStub == null) return; // The manual filling isn't needed.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            barStub.setLayoutResource(R.layout.keyboard_accessory_modern);
        } else {
            barStub.setLayoutResource(R.layout.keyboard_accessory);
        }
        sheetStub.setLayoutResource(R.layout.keyboard_accessory_sheet);
        initialize(windowAndroid, new KeyboardAccessoryCoordinator(mMediator, barStub),
                new AccessorySheetCoordinator(sheetStub));
    }

    @VisibleForTesting
    void initialize(WindowAndroid windowAndroid, KeyboardAccessoryCoordinator accessoryBar,
            AccessorySheetCoordinator accessorySheet) {
        mMediator.initialize(accessoryBar, accessorySheet, windowAndroid);
    }

    @Override
    public void destroy() {
        mMediator.destroy();
    }

    @Override
    public boolean handleBackPress() {
        return mMediator.handleBackPress();
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
            PropertyProvider<KeyboardAccessoryData.Action[]> actionProvider) {
        mMediator.registerActionProvider(actionProvider);
    }

    @Override
    public void registerSheetDataProvider(@AccessoryTabType int sheetType,
            PropertyProvider<KeyboardAccessoryData.AccessorySheetData> sheetDataProvider) {
        mMediator.registerSheetDataProvider(sheetType, sheetDataProvider);
    }

    @Override
    public void registerAutofillProvider(
            PropertyProvider<AutofillSuggestion[]> autofillProvider, AutofillDelegate delegate) {
        mMediator.registerAutofillProvider(autofillProvider, delegate);
    }

    @Override
    public void showWhenKeyboardIsVisible() {
        mMediator.showWhenKeyboardIsVisible();
    }

    @Override
    public void hide() {
        mMediator.hide();
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
    public CompositorViewResizer getKeyboardExtensionViewResizer() {
        return mMediator.getKeyboardExtensionViewResizer();
    }

    @Override
    public boolean isFillingViewShown(View view) {
        return mMediator.isFillingViewShown(view);
    }

    @VisibleForTesting
    ManualFillingMediator getMediatorForTesting() {
        return mMediator;
    }
}
