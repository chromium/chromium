// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.support.v7.app.AlertDialog;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillPopupBridge implements AutofillDelegate, DialogInterface.OnClickListener {
    private final long mNativeAutofillPopup;
    private final AutofillPopup mAutofillPopup;
    private AlertDialog mDeletionDialog;
    private final Context mContext;
    private WebContentsAccessibility mWebContentsAccessibility;

    public AutofillPopupBridge(View anchorView, long nativeAutofillPopupViewAndroid,
            WindowAndroid windowAndroid) {
        mNativeAutofillPopup = nativeAutofillPopupViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null || notEnoughScreenSpace(activity)) {
            mAutofillPopup = null;
            mContext = null;
        } else {
            mAutofillPopup = new AutofillPopup(activity, anchorView, this);
            mContext = activity;
            ChromeActivity chromeActivity = (ChromeActivity) activity;
            chromeActivity.getManualFillingComponent().notifyPopupAvailable(mAutofillPopup);
            mWebContentsAccessibility = WebContentsAccessibility.fromWebContents(
                    chromeActivity.getCurrentWebContents());
        }
    }

    @CalledByNative
    private static AutofillPopupBridge create(View anchorView, long nativeAutofillPopupViewAndroid,
            WindowAndroid windowAndroid) {
        return new AutofillPopupBridge(anchorView, nativeAutofillPopupViewAndroid, windowAndroid);
    }

    @Override
    public void dismissed() {
        AutofillPopupBridgeJni.get().popupDismissed(mNativeAutofillPopup, AutofillPopupBridge.this);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        AutofillPopupBridgeJni.get().suggestionSelected(
                mNativeAutofillPopup, AutofillPopupBridge.this, listIndex);
    }

    @Override
    public void deleteSuggestion(int listIndex) {
        AutofillPopupBridgeJni.get().deletionRequested(
                mNativeAutofillPopup, AutofillPopupBridge.this, listIndex);
    }

    @Override
    public void accessibilityFocusCleared() {
        mWebContentsAccessibility.onAutofillPopupAccessibilityFocusCleared();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        assert which == DialogInterface.BUTTON_POSITIVE;
        AutofillPopupBridgeJni.get().deletionConfirmed(
                mNativeAutofillPopup, AutofillPopupBridge.this);
    }

    /**
     * Hides the Autofill Popup and removes its anchor from the ContainerView.
     */
    @CalledByNative
    private void dismiss() {
        if (mAutofillPopup != null) mAutofillPopup.dismiss();
        if (mDeletionDialog != null) mDeletionDialog.dismiss();
        mWebContentsAccessibility.onAutofillPopupDismissed();
    }

    /**
     * Shows an Autofill popup with specified suggestions.
     * @param suggestions Autofill suggestions to be displayed.
     * @param isRtl @code true if right-to-left text.
     */
    @CalledByNative
    private void show(AutofillSuggestion[] suggestions, boolean isRtl) {
        if (mAutofillPopup != null) {
            mAutofillPopup.filterAndShow(suggestions, isRtl, shouldUseRefreshStyle());
            mWebContentsAccessibility.onAutofillPopupDisplayed(mAutofillPopup.getListView());
        }
    }

    @CalledByNative
    private void confirmDeletion(String title, String body) {
        mDeletionDialog =
                new UiUtils
                        .CompatibleAlertDialogBuilder(mContext, R.style.Theme_Chromium_AlertDialog)
                        .setTitle(title)
                        .setMessage(body)
                        .setNegativeButton(R.string.cancel, null)
                        .setPositiveButton(R.string.ok, this)
                        .create();
        mDeletionDialog.show();
    }

    @CalledByNative
    private boolean wasSuppressed() {
        return mAutofillPopup == null;
    }

    private static boolean shouldUseRefreshStyle() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_REFRESH_STYLE_ANDROID);
    }

    private static boolean notEnoughScreenSpace(Context context) {
        Configuration config = context.getResources().getConfiguration();
        // In landscape mode, most vertical space is used by the on-screen keyboard. When refresh
        // style is used, the footer is sticky, so there is not much space to even show the first
        // suggestion. In those cases, the dropdown should only be shown on very large screen
        // devices, such as tablets.
        //
        // TODO(crbug.com/907634): This is a simple first approach to not provide a degraded
        //                         experience. Explore other alternatives when this happens such as
        //                         showing suggestions on the keyboard accessory.
        return shouldUseRefreshStyle() && config.orientation == Configuration.ORIENTATION_LANDSCAPE
                && !config.isLayoutSizeAtLeast(Configuration.SCREENLAYOUT_SIZE_XLARGE);
    }

    // Helper methods for AutofillSuggestion

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param label First line of the suggestion.
     * @param sublabel Second line of the suggestion.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param isIconAtStart {@code true} if {@param iconId} is displayed before {@param label}.
     * @param suggestionId Identifier for the suggestion type.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param isLabelMultiline Whether the label should be should over multiple lines.
     * @param isLabelBold true if {@param label} should be displayed in {@code Typeface.BOLD},
     * false if {@param label} should be displayed in {@code Typeface.NORMAL}.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(AutofillSuggestion[] array, int index,
            String label, String sublabel, int iconId, boolean isIconAtStart,
            int suggestionId, boolean isDeletable, boolean isLabelMultiline, boolean isLabelBold) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : ResourceId.mapToDrawableId(iconId);
        array[index] = new AutofillSuggestion(label, sublabel, drawableId, isIconAtStart,
                suggestionId, isDeletable, isLabelMultiline, isLabelBold);
    }

    @NativeMethods
    interface Natives {
        void suggestionSelected(
                long nativeAutofillPopupViewAndroid, AutofillPopupBridge caller, int listIndex);
        void deletionRequested(
                long nativeAutofillPopupViewAndroid, AutofillPopupBridge caller, int listIndex);
        void deletionConfirmed(long nativeAutofillPopupViewAndroid, AutofillPopupBridge caller);
        void popupDismissed(long nativeAutofillPopupViewAndroid, AutofillPopupBridge caller);
    }
}
