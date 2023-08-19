// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupItemId;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.ViewAndroidDelegate;
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
    private final WebContentsAccessibility mWebContentsAccessibility;
    private final WebContentsViewRectProvider mWebContentsViewRectProvider;

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
        if (activity == null || webContents == null) {
            mAutofillPopup = null;
            mContext = null;
            mWebContentsViewRectProvider = null;
            mWebContentsAccessibility = null;
        } else {
            mContext = activity;
            ObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier =
                    ManualFillingComponentSupplier.from(windowAndroid);
            mWebContentsViewRectProvider = tryCreateRectProvider(webContents, windowAndroid);
            mAutofillPopup =
                    new AutofillPopup(activity, anchorView, this, mWebContentsViewRectProvider);
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
        if (mWebContentsViewRectProvider != null) mWebContentsViewRectProvider.dismiss();
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
            mAutofillPopup.filterAndShow(suggestions, isRtl);
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
     * @param popupItemId Determines the suggestion type.
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
            int iconId, boolean isIconAtStart, @PopupItemId int popupItemId, boolean isDeletable,
            boolean isLabelMultiline, boolean isLabelBold, GURL customIconUrl) {
        array[index] = new AutofillSuggestion.Builder()
                               .setLabel(label)
                               .setSecondaryLabel(secondaryLabel)
                               .setSubLabel(sublabel)
                               .setSecondarySubLabel(secondarySublabel)
                               .setItemTag(itemTag)
                               .setIsIconAtStart(isIconAtStart)
                               .setPopupItemId(popupItemId)
                               .setIsDeletable(isDeletable)
                               .setIsMultiLineLabel(isLabelMultiline)
                               .setIsBoldLabel(isLabelBold)
                               .setIconDrawable(AutofillUiUtils.getCardIcon(mContext, customIconUrl,
                                       iconId, AutofillUiUtils.CardIconSize.LARGE,
                                       /* showCustomIcon= */ true))
                               .build();
    }

    private @Nullable WebContentsViewRectProvider tryCreateRectProvider(
            WebContents webContents, WindowAndroid windowAndroid) {
        ObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier =
                ManualFillingComponentSupplier.from(windowAndroid);
        ViewAndroidDelegate viewDelegate = webContents.getViewAndroidDelegate();
        if (viewDelegate == null || viewDelegate.getContainerView() == null) return null;
        return new WebContentsViewRectProvider(webContents,
                BrowserControlsManagerSupplier.from(windowAndroid), manualFillingComponentSupplier);
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
