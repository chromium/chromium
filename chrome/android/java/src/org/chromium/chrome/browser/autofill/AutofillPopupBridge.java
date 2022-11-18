// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillPopupBridge implements AutofillDelegate, DialogInterface.OnClickListener {
    private long mNativeAutofillPopup;
    private final AutofillPopup mAutofillPopup;
    private AlertDialog mDeletionDialog;
    private final Context mContext;
    private WebContentsAccessibility mWebContentsAccessibility;

    public AutofillPopupBridge(@NonNull View anchorView, long nativeAutofillPopupViewAndroid,
            @NonNull WindowAndroid windowAndroid) {
        mNativeAutofillPopup = nativeAutofillPopupViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        // currentTab may be null if the last tab has been closed by the time
        // this function is called (e.g. when autofill suggestions are available,
        // see crbug.com/1315617). Its web contents may be null for a frozen
        // tab.
        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(windowAndroid);
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;
        if (activity == null || notEnoughScreenSpace(activity) || webContents == null) {
            mAutofillPopup = null;
            mContext = null;
        } else {
            mAutofillPopup = new AutofillPopup(activity, anchorView, this);
            mContext = activity;

            Supplier<ManualFillingComponent> manualFillingComponentSupplier =
                    ManualFillingComponentSupplier.from(windowAndroid);
            // Could be null if this ctor is called as the activity is being destroyed.
            if (manualFillingComponentSupplier != null
                    && manualFillingComponentSupplier.hasValue()) {
                manualFillingComponentSupplier.get().notifyPopupAvailable(mAutofillPopup);
            }

            mWebContentsAccessibility = WebContentsAccessibility.fromWebContents(webContents);
        }
    }

    @CalledByNative
    private static AutofillPopupBridge create(View anchorView, long nativeAutofillPopupViewAndroid,
            WindowAndroid windowAndroid) {
        return new AutofillPopupBridge(anchorView, nativeAutofillPopupViewAndroid, windowAndroid);
    }

    @Override
    public void dismissed() {
        if (mNativeAutofillPopup == 0) return;
        AutofillPopupBridgeJni.get().popupDismissed(mNativeAutofillPopup, AutofillPopupBridge.this);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        if (mNativeAutofillPopup == 0) return;
        AutofillPopupBridgeJni.get().suggestionSelected(
                mNativeAutofillPopup, AutofillPopupBridge.this, listIndex);
    }

    @Override
    public void deleteSuggestion(int listIndex) {
        if (mNativeAutofillPopup == 0) return;
        AutofillPopupBridgeJni.get().deletionRequested(
                mNativeAutofillPopup, AutofillPopupBridge.this, listIndex);
    }

    @Override
    public void accessibilityFocusCleared() {
        mWebContentsAccessibility.onAutofillPopupAccessibilityFocusCleared();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (mNativeAutofillPopup == 0) return;
        assert which == DialogInterface.BUTTON_POSITIVE;
        AutofillPopupBridgeJni.get().deletionConfirmed(
                mNativeAutofillPopup, AutofillPopupBridge.this);
    }

    /**
     * Hides the Autofill Popup and removes its anchor from the ContainerView.
     */
    @CalledByNative
    private void dismiss() {
        mNativeAutofillPopup = 0;
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
                new AlertDialog.Builder(mContext, R.style.ThemeOverlay_BrowserUI_AlertDialog)
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
     * @param label The first part of the first line of the suggestion.
     * @param secondaryLabel The second part of the first line of the suggestion.
     * @param sublabel The first part of the second line of the suggestion.
     * @param secondarySublabel The second part of the second line of the suggestion.
     * @param itemTag The third line of the suggestion.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param isIconAtStart {@code true} if {@param iconId} is displayed before {@param label}.
     * @param suggestionId Identifier for the suggestion type.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param isLabelMultiline Whether the label should be should over multiple lines.
     * @param isLabelBold true if {@param label} should be displayed in {@code Typeface.BOLD},
     * false if {@param label} should be displayed in {@code Typeface.NORMAL}.
     * @param customIconUrl Url for the icon to be displayed in the autofill suggestion. If present,
     *         it'd be preferred over the iconId.
     */
    @CalledByNative
    private void addToAutofillSuggestionArray(AutofillSuggestion[] array, int index, String label,
            String secondaryLabel, String sublabel, String secondarySublabel, String itemTag,
            int iconId, boolean isIconAtStart, int suggestionId, boolean isDeletable,
            boolean isLabelMultiline, boolean isLabelBold, GURL customIconUrl) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : iconId;
        AutofillSuggestion.Builder builder = new AutofillSuggestion.Builder()
                                                     .setLabel(label)
                                                     .setSecondaryLabel(secondaryLabel)
                                                     .setSubLabel(sublabel)
                                                     .setSecondarySubLabel(secondarySublabel)
                                                     .setItemTag(itemTag)
                                                     .setIconId(drawableId)
                                                     .setIsIconAtStart(isIconAtStart)
                                                     .setSuggestionId(suggestionId)
                                                     .setIsDeletable(isDeletable)
                                                     .setIsMultiLineLabel(isLabelMultiline)
                                                     .setIsBoldLabel(isLabelBold);
        if (customIconUrl != null && customIconUrl.isValid()) {
            builder.setCustomIcon(
                    PersonalDataManager.getInstance()
                            .getCustomImageForAutofillSuggestionIfAvailable(
                                    AutofillUiUtils.getCCIconURLWithParams(customIconUrl,
                                            mContext.getResources().getDimensionPixelSize(
                                                    R.dimen.autofill_dropdown_icon_width),
                                            mContext.getResources().getDimensionPixelSize(
                                                    R.dimen.autofill_dropdown_icon_height))));
        }
        array[index] = builder.build();
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
