// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;

/** JNI call glue between C++ (AutofillKeyboardAccessoryViewImpl) and Java objects. */
@JNINamespace("autofill")
public class AutofillKeyboardAccessoryViewBridge implements AutofillDelegate {
    private long mNativeAutofillKeyboardAccessory;
    private @Nullable ObservableSupplier<ManualFillingComponent> mManualFillingComponentSupplier;
    private @Nullable ManualFillingComponent mManualFillingComponent;
    private @Nullable Context mContext;
    private final PropertyProvider<List<AutofillSuggestion>> mChipProvider =
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

    private void onDeletionDialogClosed(boolean confirmed) {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .onDeletionDialogClosed(
                        mNativeAutofillKeyboardAccessory,
                        AutofillKeyboardAccessoryViewBridge.this,
                        confirmed);
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
            mChipProvider.notifyObservers(List.of());
            mManualFillingComponentSupplier.removeObserver(mFillingComponentObserver);
        }
        dismissed();
        mContext = null;
    }

    /**
     * Shows an Autofill view with specified suggestions.
     *
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show(@JniType("std::vector") List<AutofillSuggestion> suggestions) {
        mChipProvider.notifyObservers(suggestions);
    }

    @CalledByNative
    private void confirmDeletion(
            @JniType("std::u16string") String title, @JniType("std::u16string") String body) {
        assert mManualFillingComponent != null;
        mManualFillingComponent.confirmOperation(
                title,
                body,
                () -> this.onDeletionDialogClosed(/* confirmed= */ true),
                () -> this.onDeletionDialogClosed(/* confirmed= */ false));
    }

    /**
     * Creates an Autofill suggestion.
     *
     * @param label Suggested text. The text that's going to be filled in the focused field, with a
     *     few exceptions:
     *     <ul>
     *       <li>Credit card numbers are elided, e.g. "Visa ****-1234."
     *       <li>The text "CLEAR FORM" will clear the filled in text.
     *       <li>Empty text can be used to display only icons, e.g. for credit card scan or editing
     *           autofill settings.
     *     </ul>
     *
     * @param sublabel Hint for the suggested text. The text that's going to be filled in the
     *     unfocused fields of the form. If {@see label} is empty, then this must be empty too.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param suggestionType Determines the type of the suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param featureForIPH The In-Product-Help feature used for displaying the bubble for the
     *     suggestion.
     * @param customIconUrl The url used to fetch the custom icon to be displayed in the autofill
     *     suggestion chip.
     * @return an AutofillSuggestion containing the above information.
     */
    @CalledByNative
    private static AutofillSuggestion createAutofillSuggestion(
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String sublabel,
            int iconId,
            @SuggestionType int suggestionType,
            boolean isDeletable,
            @JniType("std::string") String featureForIPH,
            @JniType("std::u16string") String iphDescriptionText,
            GURL customIconUrl,
            boolean applyDeactivatedStyle) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : iconId;
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSubLabel(sublabel)
                .setIconId(drawableId)
                .setIsIconAtStart(false)
                .setSuggestionType(suggestionType)
                .setIsDeletable(isDeletable)
                .setIsMultiLineLabel(false)
                .setIsBoldLabel(false)
                .setFeatureForIPH(featureForIPH)
                .setIPHDescriptionText(iphDescriptionText)
                .setCustomIconUrl(customIconUrl)
                .setApplyDeactivatedStyle(applyDeactivatedStyle)
                .build();
    }

    /**
     * Used to register the filling component that receives and renders the autofill suggestions.
     * Noop if the component hasn't changed or became null.
     *
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
                long nativeAutofillKeyboardAccessoryViewImpl,
                AutofillKeyboardAccessoryViewBridge caller);

        void suggestionSelected(
                long nativeAutofillKeyboardAccessoryViewImpl,
                AutofillKeyboardAccessoryViewBridge caller,
                int listIndex);

        void deletionRequested(
                long nativeAutofillKeyboardAccessoryViewImpl,
                AutofillKeyboardAccessoryViewBridge caller,
                int listIndex);

        void onDeletionDialogClosed(
                long nativeAutofillKeyboardAccessoryViewImpl,
                AutofillKeyboardAccessoryViewBridge caller,
                boolean confirmed);
    }
}
