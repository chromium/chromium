// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.view.View;
import android.view.ViewStub;

import org.chromium.chrome.browser.compositor.CompositorViewResizer;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.WindowAndroid;

/**
 * This component handles the new, non-popup filling UI.
 */
public interface ManualFillingComponent {
    /**
     * Initializes the manual filling component. Calls to this class are NoOps until this method
     * is called.
     * @param windowAndroid The window needed to listen to the keyboard and to connect to
     *         activity.
     * @param barStub The {@link ViewStub} used to inflate the keyboard accessory bar.
     * @param sheetStub The {@link ViewStub} used to inflate the keyboard accessory bottom
     *         sheet.
     */
    void initialize(WindowAndroid windowAndroid, ViewStub barStub, ViewStub sheetStub);

    /**
     * Cleans up the manual UI by destroying the accessory bar and its bottom sheet.
     */
    void destroy();

    /**
     * Handles tapping on the Android back button.
     * @return Whether tapping the back button dismissed the accessory sheet or not.
     */
    boolean handleBackPress();

    /**
     * Ensures that keyboard accessory and keyboard are hidden and reset.
     */
    void dismiss();

    /**
     * Notifies the component that a popup window exists so it can be dismissed if necessary.
     * @param popup A {@link DropdownPopupWindow} that might be dismissed later.
     */
    void notifyPopupAvailable(DropdownPopupWindow popup);

    /**
     * By registering a provider, an empty tab of the given tab type is created. Call
     * {@link PropertyProvider#notifyObservers(Object)} to fill or update the sheet.
     * @param sheetType The type of sheet to instantiate and to provide data for.
     * @param sheetDataProvider The {@link PropertyProvider} the tab will get its data from.
     */
    void registerSheetDataProvider(@AccessoryTabType int sheetType,
            PropertyProvider<KeyboardAccessoryData.AccessorySheetData> sheetDataProvider);

    /**
     * Registers a provider, to provide actions for the keyboard accessory bar. Call
     * {@link PropertyProvider#notifyObservers(Object)} to fill or update the actions.
     * @param actionProvider The {@link PropertyProvider} providing actions.
     */
    void registerActionProvider(PropertyProvider<KeyboardAccessoryData.Action[]> actionProvider);

    /**
     * Registers a provider, to provide autofill suggestions for the keyboard accessory bar. Call
     * {@link PropertyProvider#notifyObservers(Object)} to fill or update the suggestions.
     * @param autofillProvider The {@link PropertyProvider} providing autofill suggestions.
     * @param delegate The {@link AutofillDelegate} to call for interaction with the suggestions.
     */
    void registerAutofillProvider(
            PropertyProvider<AutofillSuggestion[]> autofillProvider, AutofillDelegate delegate);

    /**
     * Signals that the accessory has permission to show if the user focuses a form field.
     */
    void showWhenKeyboardIsVisible();

    /**
     * Requests to close the active tab in the keyboard accessory. If there is no active tab, this
     * is a NoOp.
     */
    void closeAccessorySheet();

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    void swapSheetWithKeyboard();

    /**
     * Hides the sheet until undone with {@link #showWhenKeyboardIsVisible()}.
     */
    void hide();

    /**
     * Notifies the component that the activity it's living in was resumed.
     */
    void onResume();

    /**
     * Notifies the component that the activity it's living in was paused.
     */
    void onPause();

    /**
     * Returns a {@link CompositorViewResizer} that allows to access the combined height of
     * KeyboardAccessoryCoordinator and AccessorySheetCoordinator, and to be
     * notified when it changes.
     * @return A {@link CompositorViewResizer}.
     */
    CompositorViewResizer getKeyboardExtensionViewResizer();

    /**
     * Returns whether the Keyboard is replaced by an accessory sheet or is about to do so.
     * @return True if an accessory sheet is (being) opened and replacing the keyboard.
     * @param view A {@link View} that is used to find the window root.
     */
    boolean isFillingViewShown(View view);
}
