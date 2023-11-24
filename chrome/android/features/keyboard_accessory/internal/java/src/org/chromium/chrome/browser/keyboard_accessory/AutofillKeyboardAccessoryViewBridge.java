// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupItemId;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * JNI call glue for AutofillExternalDelagate C++ and Java objects.
 * This provides an alternative UI for Autofill suggestions, and replaces AutofillPopupBridge when
 * --enable-autofill-keyboard-accessory-view is passed on the command line.
 */
@JNINamespace("autofill")
public class AutofillKeyboardAccessoryViewBridge implements AutofillDelegate {
    private long mNativeAutofillKeyboardAccessory;
    private @Nullable ObservableSupplier<ManualFillingComponent> mManualFillingComponentSupplier;
    private @Nullable ManualFillingComponent mManualFillingComponent;
    private @Nullable Context mContext;
    private final PropertyProvider<AutofillSuggestion[]> mChipProvider =
            new PropertyProvider<>(AccessoryAction.AUTOFILL_SUGGESTION);
    private final Callback<ManualFillingComponent> mFillingComponentObserver =
            this::connectToFillingComponent;

    private AutofillKeyboardAccessoryViewBridge() {}

    @CalledByNative
    private static AutofillKeyboardAccessoryViewBridge create() {
        return new AutofillKeyboardAccessoryViewBridge();
    }

    @Override
    public void dismissed() {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .viewDismissed(
                        mNativeAutofillKeyboardAccessory, AutofillKeyboardAccessoryViewBridge.this);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        mManualFillingComponent.dismiss();
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .suggestionSelected(
                        mNativeAutofillKeyboardAccessory,
                        AutofillKeyboardAccessoryViewBridge.this,
                        listIndex);
    }

    @Override
    public void deleteSuggestion(int listIndex) {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .deletionRequested(
                        mNativeAutofillKeyboardAccessory,
                        AutofillKeyboardAccessoryViewBridge.this,
                        listIndex);
    }

    @Override
    public void accessibilityFocusCleared() {}

    private void onDeletionConfirmed() {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .deletionConfirmed(
                        mNativeAutofillKeyboardAccessory, AutofillKeyboardAccessoryViewBridge.this);
    }

    /**
     * Initializes this object.
     * This function should be called at most one time.
     * @param nativeAutofillKeyboardAccessory Handle to the native counterpart.
     * @param windowAndroid The window on which to show the suggestions.
     */
    @CalledByNative
    private void init(long nativeAutofillKeyboardAccessory, WindowAndroid windowAndroid) {
        mContext = windowAndroid.getActivity().get();
        assert mContext != null;

        mManualFillingComponentSupplier = ManualFillingComponentSupplier.from(windowAndroid);
        if (mManualFillingComponentSupplier != null) {
            ManualFillingComponent currentFillingComponent =
                    mManualFillingComponentSupplier.addObserver(mFillingComponentObserver);
            connectToFillingComponent(currentFillingComponent);
        }

        mNativeAutofillKeyboardAccessory = nativeAutofillKeyboardAccessory;
    }

    /** Clears the reference to the native view. */
    @CalledByNative
    private void resetNativeViewPointer() {
        mNativeAutofillKeyboardAccessory = 0;
    }

    /** Hides the Autofill view. */
    @CalledByNative
    private void dismiss() {
        if (mManualFillingComponentSupplier != null) {
            mChipProvider.notifyObservers(new AutofillSuggestion[0]);
            mManualFillingComponentSupplier.removeObserver(mFillingComponentObserver);
        }
        dismissed();
        mContext = null;
    }

    /**
     * Shows an Autofill view with specified suggestions.
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show(AutofillSuggestion[] suggestions) {
        mChipProvider.notifyObservers(suggestions);
    }

    @CalledByNative
    private void confirmDeletion(String title, String body) {
        assert mManualFillingComponent != null;
        mManualFillingComponent.confirmOperation(title, body, this::onDeletionConfirmed);
    }

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param label Suggested text. The text that's going to be filled in the focused field, with a
     *              few exceptions:
     *              <ul>
     *                  <li>Credit card numbers are elided, e.g. "Visa ****-1234."</li>
     *                  <li>The text "CLEAR FORM" will clear the filled in text.</li>
     *                  <li>Empty text can be used to display only icons, e.g. for credit card scan
     *                      or editing autofill settings.</li>
     *              </ul>
     * @param sublabel Hint for the suggested text. The text that's going to be filled in the
     *                 unfocused fields of the form. If {@see label} is empty, then this must be
     *                 empty too.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param popupItemId Determines the type of the suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param featureForIPH The In-Product-Help feature used for displaying the bubble for the
     *         suggestion.
     * @param customIconUrl The url used to fetch the custom icon to be displayed in the autofill
     *         suggestion chip.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(
            AutofillSuggestion[] array,
            int index,
            String label,
            String sublabel,
            int iconId,
            @PopupItemId int popupItemId,
            boolean isDeletable,
            String featureForIPH,
            GURL customIconUrl) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : iconId;
        array[index] =
                new AutofillSuggestion.Builder()
                        .setLabel(label)
                        .setSubLabel(sublabel)
                        .setIconId(drawableId)
                        .setIsIconAtStart(false)
                        .setPopupItemId(popupItemId)
                        .setIsDeletable(isDeletable)
                        .setIsMultiLineLabel(false)
                        .setIsBoldLabel(false)
                        .setFeatureForIPH(featureForIPH)
                        .setCustomIconUrl(customIconUrl)
                        .build();
    }

    /**
     * Used to register the filling component that receives and renders the autofill suggestions.
     * Noop if the component hasn't changed or became null.
     * @param fillingComponent The {@link ManualFillingComponent} displaying suggestions as chips.
     */
    private void connectToFillingComponent(@Nullable ManualFillingComponent fillingComponent) {
        if (mManualFillingComponent == fillingComponent) return;
        mManualFillingComponent = fillingComponent;
        if (mManualFillingComponent == null) return;
        mManualFillingComponent.registerAutofillProvider(mChipProvider, this);
    }

    @NativeMethods
    interface Natives {
        void viewDismissed(
                long nativeAutofillKeyboardAccessoryView,
                AutofillKeyboardAccessoryViewBridge caller);

        void suggestionSelected(
                long nativeAutofillKeyboardAccessoryView,
                AutofillKeyboardAccessoryViewBridge caller,
                int listIndex);

        void deletionRequested(
                long nativeAutofillKeyboardAccessoryView,
                AutofillKeyboardAccessoryViewBridge caller,
                int listIndex);

        void deletionConfirmed(
                long nativeAutofillKeyboardAccessoryView,
                AutofillKeyboardAccessoryViewBridge caller);
    }
}
