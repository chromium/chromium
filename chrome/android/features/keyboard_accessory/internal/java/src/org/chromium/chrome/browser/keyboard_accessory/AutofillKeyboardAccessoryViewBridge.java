// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.app.Activity;
import android.net.Uri;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.AutofillSuggestion.Payload;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.SpanApplier;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** JNI call glue between C++ (AutofillKeyboardAccessoryViewImpl) and Java objects. */
@JNINamespace("autofill")
public class AutofillKeyboardAccessoryViewBridge implements AutofillDelegate {
    private long mNativeAutofillKeyboardAccessory;
    private WeakReference<Activity> mActivity;
    private @Nullable ObservableSupplier<ManualFillingComponent> mManualFillingComponentSupplier;
    private @Nullable ManualFillingComponent mManualFillingComponent;
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
                .viewDismissed(mNativeAutofillKeyboardAccessory);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        mManualFillingComponent.dismiss();
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .suggestionSelected(mNativeAutofillKeyboardAccessory, listIndex);
    }

    @Override
    public void deleteSuggestion(int listIndex) {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .deletionRequested(mNativeAutofillKeyboardAccessory, listIndex);
    }

    @Override
    public void accessibilityFocusCleared() {}

    private void onDeletionDialogClosed(boolean confirmed) {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        AutofillKeyboardAccessoryViewBridgeJni.get()
                .onDeletionDialogClosed(mNativeAutofillKeyboardAccessory, confirmed);
    }

    private CharSequence createMessageWithLink(String body, String link) {
        if (mActivity.get() == null) {
            return body;
        }
        ClickableSpan span =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        assert mActivity.get() != null;
                        new CustomTabsIntent.Builder()
                                .setShowTitle(true)
                                .build()
                                .launchUrl(mActivity.get(), Uri.parse(link));
                    }
                };
        return SpanApplier.applySpans(body, new SpanApplier.SpanInfo("<link>", "</link>", span));
    }

    /**
     * Initializes this object. This function should be called at most one time.
     *
     * @param nativeAutofillKeyboardAccessory Handle to the native counterpart.
     * @param windowAndroid The window on which to show the suggestions.
     */
    @CalledByNative
    private void init(long nativeAutofillKeyboardAccessory, WindowAndroid windowAndroid) {
        mManualFillingComponentSupplier = ManualFillingComponentSupplier.from(windowAndroid);
        if (mManualFillingComponentSupplier != null) {
            ManualFillingComponent currentFillingComponent =
                    mManualFillingComponentSupplier.addObserver(mFillingComponentObserver);
            connectToFillingComponent(currentFillingComponent);
        }

        mActivity = windowAndroid.getActivity();
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
            if (mManualFillingComponent != null) {
                mManualFillingComponent.setSuggestions(List.of(), this);
            }
            mManualFillingComponentSupplier.removeObserver(mFillingComponentObserver);
        }
        dismissed();
    }

    /**
     * Shows an Autofill view with specified suggestions.
     *
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show(@JniType("std::vector") List<AutofillSuggestion> suggestions) {
        if (mManualFillingComponent != null) {
            mManualFillingComponent.setSuggestions(suggestions, this);
        }
    }

    /**
     * Shows a deletion confirmation dialog for a KeyboardAccessory suggestion.
     *
     * @param title The title for the dialog.
     * @param body The body of the dialog. This may contain &lt;link&gt; tags, which will be linked
     *     to {@code bodyLink}.
     * @param bodyLink If not empty, this string will be used as the link within the &lt;link&gt;
     *     tags in the body.
     * @param confirmButtonText The text displayed on the confirmation button (e.g., "Remove",
     *     "Delete").
     */
    @CalledByNative
    private void confirmDeletion(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String body,
            @JniType("std::u16string") String bodyLink,
            @JniType("std::u16string") String confirmButtonText) {

        CharSequence message = body;
        if (!bodyLink.isEmpty() && mActivity.get() != null) {
            message = createMessageWithLink(body, bodyLink);
        }

        assert mManualFillingComponent != null;
        mManualFillingComponent.confirmDeletionOperation(
                title,
                message,
                confirmButtonText,
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
     * @param voiceOver Voice over text read for the keyboard accessory suggestion.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param suggestionType Determines the type of the suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param featureForIph The In-Product-Help feature used for displaying the bubble for the
     *     suggestion.
     * @param iphDescriptionText If set, it will be used as the help text for the IPH bubble.
     * @param customIconUrl The url used to fetch the custom icon to be displayed in the autofill
     *     suggestion chip.
     * @return an AutofillSuggestion containing the above information.
     */
    @CalledByNative
    private static AutofillSuggestion createAutofillSuggestion(
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String sublabel,
            @JniType("std::u16string") String voiceOver,
            int iconId,
            @SuggestionType int suggestionType,
            boolean isDeletable,
            @JniType("std::string") String featureForIph,
            @JniType("std::u16string") String iphDescriptionText,
            GURL customIconUrl,
            boolean applyDeactivatedStyle,
            @Nullable Payload payload) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : iconId;
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSubLabel(sublabel)
                .setVoiceOver(voiceOver)
                .setIconId(drawableId)
                .setSuggestionType(suggestionType)
                .setIsDeletable(isDeletable)
                .setFeatureForIph(featureForIph)
                .setIphDescriptionText(iphDescriptionText)
                .setCustomIconUrl(customIconUrl)
                .setApplyDeactivatedStyle(applyDeactivatedStyle)
                .setPayload(payload)
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
    }

    @NativeMethods
    interface Natives {
        void viewDismissed(long nativeAutofillKeyboardAccessoryViewImpl);

        void suggestionSelected(long nativeAutofillKeyboardAccessoryViewImpl, int listIndex);

        void deletionRequested(long nativeAutofillKeyboardAccessoryViewImpl, int listIndex);

        void onDeletionDialogClosed(
                long nativeAutofillKeyboardAccessoryViewImpl, boolean confirmed);
    }
}
