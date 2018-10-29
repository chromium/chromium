// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.graphics.Bitmap;
import android.support.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.ui.base.WindowAndroid;

class PasswordAccessoryBridge {
    private final KeyboardAccessoryData.PropertyProvider<Item> mItemProvider =
            new KeyboardAccessoryData.PropertyProvider<>();
    private final KeyboardAccessoryData.PropertyProvider<Action> mActionProvider =
            new KeyboardAccessoryData.PropertyProvider<>(
                    AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
    private final ManualFillingCoordinator mManualFillingCoordinator;
    private final ChromeActivity mActivity;
    private long mNativeView;

    private PasswordAccessoryBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        mManualFillingCoordinator = mActivity.getManualFillingController();
        mManualFillingCoordinator.registerPasswordProvider(mItemProvider);
        mManualFillingCoordinator.registerActionProvider(mActionProvider);
    }

    @CalledByNative
    private static PasswordAccessoryBridge create(long nativeView, WindowAndroid windowAndroid) {
        return new PasswordAccessoryBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void onItemsAvailable(
            String[] text, String[] description, int[] isPassword, int[] itemType) {
        mItemProvider.notifyObservers(convertToItems(text, description, isPassword, itemType));
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
                        KeyboardAccessoryMetricsRecorder.recordActionSelected(
                                AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
                        nativeOnGenerationRequested(mNativeView);
                    })};
        } else {
            generationAction = new Action[0];
        }
        mActionProvider.notifyObservers(generationAction);
    }

    @CalledByNative
    void showWhenKeyboardIsVisible() {
        mManualFillingCoordinator.showWhenKeyboardIsVisible();
    }

    @CalledByNative
    void hide() {
        mManualFillingCoordinator.hide();
    }

    @CalledByNative
    private void closeAccessorySheet() {
        mManualFillingCoordinator.closeAccessorySheet();
    }

    @CalledByNative
    private void swapSheetWithKeyboard() {
        mManualFillingCoordinator.swapSheetWithKeyboard();
    }

    @CalledByNative
    private void destroy() {
        mItemProvider.notifyObservers(new Item[] {}); // There are no more items available!
        mNativeView = 0;
    }

    private Item[] convertToItems(
            String[] text, String[] description, int[] isPassword, int[] type) {
        Item[] items = new Item[text.length];
        for (int i = 0; i < text.length; i++) {
            switch (type[i]) {
                case ItemType.LABEL:
                    items[i] = Item.createLabel(text[i], description[i]);
                    continue;
                case ItemType.SUGGESTION:
                    items[i] = Item.createSuggestion(
                            text[i], description[i], isPassword[i] == 1, (item) -> {
                                assert mNativeView
                                        != 0 : "Controller was destroyed but the bridge wasn't!";
                                KeyboardAccessoryMetricsRecorder.recordSuggestionSelected(
                                        AccessoryTabType.PASSWORDS,
                                        item.isPassword() ? AccessorySuggestionType.PASSWORD
                                                          : AccessorySuggestionType.USERNAME);
                                nativeOnFillingTriggered(
                                        mNativeView, item.isPassword(), item.getCaption());
                            }, this::fetchFavicon);
                    continue;
                case ItemType.NON_INTERACTIVE_SUGGESTION:
                    items[i] = Item.createSuggestion(
                            text[i], description[i], isPassword[i] == 1, null, this::fetchFavicon);
                    continue;
                case ItemType.DIVIDER:
                    items[i] = Item.createDivider();
                    continue;
                case ItemType.OPTION:
                    items[i] = Item.createOption(text[i], description[i], (item) -> {
                        assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                        nativeOnOptionSelected(mNativeView, item.getCaption());
                    });
                    continue;
                case ItemType.TOP_DIVIDER:
                    items[i] = Item.createTopDivider();
                    continue;
            }
            assert false : "Cannot create item for type '" + type[i] + "'.";
        }
        return items;
    }

    public void fetchFavicon(@Px int desiredSize, Callback<Bitmap> faviconCallback) {
        assert mNativeView != 0 : "Favicon was requested after the bridge was destroyed!";
        nativeOnFaviconRequested(mNativeView, desiredSize, faviconCallback);
    }

    private native void nativeOnFaviconRequested(long nativePasswordAccessoryViewAndroid,
            int desiredSizeInPx, Callback<Bitmap> faviconCallback);
    private native void nativeOnFillingTriggered(
            long nativePasswordAccessoryViewAndroid, boolean isPassword, String textToFill);
    private native void nativeOnOptionSelected(
            long nativePasswordAccessoryViewAndroid, String selectedOption);
    private native void nativeOnGenerationRequested(long nativePasswordAccessoryViewAndroid);
}