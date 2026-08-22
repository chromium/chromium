// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Activity;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.permissions.ItemChooserDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.util.ColorUtils;

/**
 * A dialog for showing available HID devices. This dialog is shown when a website requests to
 * connect to a HID device (e.g. through a hid.requestDevice Javascript call).
 */
@NullMarked
public class HidChooserDialog implements ItemChooserDialog.ItemSelectedCallback {
    /** The dialog to show to let the user pick a device. */
    private @Nullable ItemChooserDialog mItemChooserDialog;

    /** A pointer back to the native part of the implementation for this dialog. */
    private long mNativeHidChooserDialogPtr;

    /** The current profile when the dialog is created. */
    private final Profile mProfile;

    /** Creates the HidChooserDialog. */
    @VisibleForTesting
    HidChooserDialog(long nativeHidChooserDialogPtr, Profile profile) {
        mNativeHidChooserDialogPtr = nativeHidChooserDialogPtr;
        mProfile = profile;
    }

    /**
     * Shows the HidChooserDialog.
     *
     * @param activity Activity which is used for launching a dialog.
     * @param origin The origin for the site wanting to connect to the HID device.
     * @param securityLevel The security level of the connection to the site wanting to connect to
     *     the HID device. For valid values see SecurityStateModel::SecurityLevel.
     */
    @VisibleForTesting
    @Initializer
    void show(Activity activity, String origin, int securityLevel) {
        // Emphasize the origin.
        SpannableString originSpannableString = new SpannableString(origin);

        final boolean useDarkColors = !ColorUtils.inNightMode(activity);

        ChromeAutocompleteSchemeClassifier chromeAutocompleteSchemeClassifier =
                new ChromeAutocompleteSchemeClassifier(mProfile);
        OmniboxUrlEmphasizer.emphasizeUrl(
                originSpannableString,
                activity,
                chromeAutocompleteSchemeClassifier,
                securityLevel,
                useDarkColors,
                /* emphasizeScheme= */ true);
        chromeAutocompleteSchemeClassifier.destroy();
        // Construct a full string and replace the origin text with emphasized version.
        SpannableString title =
                new SpannableString(activity.getString(R.string.hid_chooser_dialog_prompt, origin));
        int start = title.toString().indexOf(origin);
        if (start != -1) {
            TextUtils.copySpansFrom(
                    originSpannableString,
                    0,
                    originSpannableString.length(),
                    Object.class,
                    title,
                    start);
        }

        String searching = "";
        String noneFound = activity.getString(R.string.hid_chooser_dialog_no_devices_found_prompt);
        SpannableString statusActive =
                SpanApplier.applySpans(
                        activity.getString(R.string.hid_chooser_dialog_footnote_text),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        activity,
                                        (view) -> {
                                            if (mNativeHidChooserDialogPtr == 0) return;

                                            Natives jni = HidChooserDialogJni.get();
                                            jni.loadHidHelpPage(mNativeHidChooserDialogPtr);

                                            // Get rid of the highlight background on selection.
                                            view.invalidate();
                                        })));
        SpannableString statusIdleNoneFound = statusActive;
        SpannableString statusIdleSomeFound = statusActive;
        String positiveButton = activity.getString(R.string.hid_chooser_dialog_connect_button_text);

        ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(
                        title,
                        searching,
                        noneFound,
                        statusActive,
                        statusIdleNoneFound,
                        statusIdleSomeFound,
                        positiveButton);
        mItemChooserDialog = new ItemChooserDialog(activity, activity.getWindow(), this, labels);
    }

    @Override
    public void onItemSelected(String id) {
        if (mNativeHidChooserDialogPtr != 0) {
            long nativePtr = mNativeHidChooserDialogPtr;
            mNativeHidChooserDialogPtr = 0;
            Natives jni = HidChooserDialogJni.get();
            if (id.isEmpty()) {
                jni.onDialogCancelled(nativePtr);
            } else {
                jni.onItemSelected(nativePtr, id);
            }
        }
    }

    @CalledByNative
    @VisibleForTesting
    static @Nullable HidChooserDialog create(
            WindowAndroid windowAndroid,
            @JniType("std::u16string") String origin,
            int securityLevel,
            Profile profile,
            long nativeHidChooserDialogPtr) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;

        // Avoid showing the chooser when ModalDialogManager indicates that
        // tab-modal or app-modal dialogs are suspended.
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (modalDialogManager != null
                && (modalDialogManager.isSuspended(ModalDialogManager.ModalDialogType.TAB)
                        || modalDialogManager.isSuspended(
                                ModalDialogManager.ModalDialogType.APP))) {
            return null;
        }

        HidChooserDialog dialog = new HidChooserDialog(nativeHidChooserDialogPtr, profile);
        dialog.show(activity, origin, securityLevel);
        return dialog;
    }

    @CalledByNative
    private void setIdleState() {
        if (mItemChooserDialog != null) {
            mItemChooserDialog.setIdleState();
        }
    }

    @VisibleForTesting
    @CalledByNative
    void addDevice(
            @JniType("std::string") String deviceId, @JniType("std::u16string") String deviceName) {
        if (mItemChooserDialog != null) {
            mItemChooserDialog.addOrUpdateItem(deviceId, deviceName);
        }
    }

    @CalledByNative
    private void removeDevice(@JniType("std::string") String deviceId) {
        if (mItemChooserDialog != null) {
            mItemChooserDialog.removeItemFromList(deviceId);
        }
    }

    @CalledByNative
    private void closeDialog() {
        mNativeHidChooserDialogPtr = 0;
        if (mItemChooserDialog != null) {
            mItemChooserDialog.dismiss();
        }
    }

    @NativeMethods
    interface Natives {
        void onItemSelected(
                long nativeHidChooserDialogAndroid, @JniType("std::string") String deviceId);

        void onDialogCancelled(long nativeHidChooserDialogAndroid);

        void loadHidHelpPage(long nativeHidChooserDialogAndroid);
    }
}
