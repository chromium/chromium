// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;
import android.support.v4.view.ViewPager;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Provider;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles requests to the manual UI for filling passwords, payments and other user data. Ideally,
 * the caller has no access to Keyboard accessory or sheet and is only interacting with this
 * component.
 * For that, it facilitates the communication between {@link KeyboardAccessoryCoordinator} and
 * {@link AccessorySheetCoordinator} to add and trigger surfaces that may assist users while filling
 * fields.
 */
public class ManualFillingCoordinator {
    private final ManualFillingMediator mMediator = new ManualFillingMediator();

    /**
     * Initializes the manual filling component. Calls to this class are NoOps until this method is
     * called.
     * @param windowAndroid The window needed to listen to the keyboard and to connect to activity.
     * @param accessoryViewProvider The view provider for the keyboard accessory bar.
     * @param viewPagerProvider The view provider for the keyboard accessory bottom sheet.
     */
    public void initialize(WindowAndroid windowAndroid,
            ViewProvider<KeyboardAccessoryView> accessoryViewProvider,
            ViewProvider<ViewPager> viewPagerProvider) {
        KeyboardAccessoryCoordinator keyboardAccessory =
                new KeyboardAccessoryCoordinator(mMediator, accessoryViewProvider);
        viewPagerProvider.whenLoaded(viewPager -> {
            accessoryViewProvider.whenLoaded(accessoryView -> {
                viewPager.addOnPageChangeListener(accessoryView.getPageChangeListener());
            });
        });
        AccessorySheetCoordinator accessorySheet = new AccessorySheetCoordinator(viewPagerProvider);
        mMediator.initialize(keyboardAccessory, accessorySheet, windowAndroid);
    }

    /**
     * Cleans up the manual UI by destroying the accessory bar and its bottom sheet.
     */
    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Handles tapping on the Android back button.
     * @return Whether tapping the back button dismissed the accessory sheet or not.
     */
    public boolean handleBackPress() {
        return mMediator.handleBackPress();
    }

    /**
     * Ensures that keyboard accessory and keyboard are hidden and reset.
     */
    public void dismiss() {
        mMediator.dismiss();
    }

    /**
     * Notifies the component that a popup window exists so it can be dismissed if necessary.
     * @param popup A {@link DropdownPopupWindow} that might be dismissed later.
     */
    public void notifyPopupAvailable(DropdownPopupWindow popup) {
        mMediator.notifyPopupOpened(popup);
    }

    /**
     * Requests to close the active tab in the keyboard accessory. If there is no active tab, this
     * is a NoOp.
     */
    public void closeAccessorySheet() {
        mMediator.onCloseAccessorySheet();
    }

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    public void swapSheetWithKeyboard() {
        mMediator.swapSheetWithKeyboard();
    }

    void registerActionProvider(
            KeyboardAccessoryData.PropertyProvider<KeyboardAccessoryData.Action> actionProvider) {
        mMediator.registerActionProvider(actionProvider);
    }

    void registerPasswordProvider(Provider<KeyboardAccessoryData.Item> itemProvider) {
        mMediator.registerPasswordProvider(itemProvider);
    }

    public void showWhenKeyboardIsVisible() {
        mMediator.showWhenKeyboardIsVisible();
    }

    public void hide() {
        mMediator.hide();
    }

    public void onResume() {
        mMediator.resume();
    }

    public void onPause() {
        mMediator.pause();
    }

    /**
     * Returns a size manager that allows to access the combined height of
     * {@link KeyboardAccessoryCoordinator} and {@link AccessorySheetCoordinator}, and to be
     * notified when it changes.
     * @return A {@link KeyboardExtensionSizeManager}.
     */
    public KeyboardExtensionSizeManager getKeyboardExtensionSizeManager() {
        return mMediator.getKeyboardExtensionSizeManager();
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    /**
     * Allows access to the keyboard accessory. This can be used to explicitly modify the the bar of
     * the keyboard accessory (e.g. by providing suggestions or actions).
     * @return The coordinator of the Keyboard accessory component.
     */
    public @Nullable KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mMediator.getKeyboardAccessory();
    }

    @VisibleForTesting
    ManualFillingMediator getMediatorForTesting() {
        return mMediator;
    }

    /**
     * Returns whether - at this very moment - the Keyboard is replaced by an accessory sheet.
     * @return True if an accessory sheet is open and replacing the keyboard.
     */
    public boolean isFillingViewShown() {
        return mMediator.isFillingViewShown();
    }
}