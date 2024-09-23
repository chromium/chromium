// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Activity;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.permissions.ItemChooserDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.util.ColorUtils;

/**
 * A dialog for showing available USB devices. This dialog is shown when a website requests to
 * connect to a USB device (e.g. through a usb.requestDevice Javascript call).
 */
public class UsbChooserDialog implements ItemChooserDialog.ItemSelectedCallback {
    /** The dialog to show to let the user pick a device. */
    ItemChooserDialog mItemChooserDialog;

    /** A pointer back to the native part of the implementation for this dialog. */
    long mNativeUsbChooserDialogPtr;

    /** The current profile when the dialog is created. */
    private final Profile mProfile;

    /** Creates the UsbChooserDialog. */
    @VisibleForTesting
    UsbChooserDialog(long nativeUsbChooserDialogPtr, Profile profile) {
        mNativeUsbChooserDialogPtr = nativeUsbChooserDialogPtr;
        mProfile = profile;
    }

    /**
     * Shows the UsbChooserDialog.
     *
     * @param activity Activity which is used for launching a dialog.
     * @param origin The origin for the site wanting to connect to the USB device.
     * @param securityLevel The security level of the connection to the site wanting to connect to
     *                      the USB device. For valid values see SecurityStateModel::SecurityLevel.
     */
    @VisibleForTesting
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
                /* emphasizeHttpsScheme= */ true);
        chromeAutocompleteSchemeClassifier.destroy();
        // Construct a full string and replace the origin text with emphasized version.
        SpannableString title =
                new SpannableString(activity.getString(R.string.usb_chooser_dialog_prompt, origin));
        int start = title.toString().indexOf(origin);
        TextUtils.copySpansFrom(
                originSpannableString,
                0,
                originSpannableString.length(),
                Object.class,
                title,
                start);

        String searching = "";
        String noneFound = activity.getString(R.string.usb_chooser_dialog_no_devices_found_prompt);
        SpannableString statusActive =
                SpanApplier.applySpans(
                        activity.getString(R.string.usb_chooser_dialog_footnote_text),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        activity,
                                        (view) -> {
                                            if (mNativeUsbChooserDialogPtr == 0) return;

                                            Natives jni = UsbChooserDialogJni.get();
                                            jni.loadUsbHelpPage(mNativeUsbChooserDialogPtr);

                                            // Get rid of the highlight background on selection.
                                            view.invalidate();
                                        })));
        SpannableString statusIdleNoneFound = statusActive;
        SpannableString statusIdleSomeFound = statusActive;
        String positiveButton = activity.getString(R.string.usb_chooser_dialog_connect_button_text);

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
        if (mNativeUsbChooserDialogPtr != 0) {
            Natives jni = UsbChooserDialogJni.get();
            if (id.isEmpty()) {
                jni.onDialogCancelled(mNativeUsbChooserDialogPtr);
            } else {
                jni.onItemSelected(mNativeUsbChooserDialogPtr, id);
            }
        }
    }

    @CalledByNative
    @VisibleForTesting
    static UsbChooserDialog create(
            WindowAndroid windowAndroid,
            String origin,
            int securityLevel,
            Profile profile,
            long nativeUsbChooserDialogPtr) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;

        // Avoid showing the chooser when ModalDialogManager indicates that
        // tab-modal or app-modal dialogs are suspended.
        // TODO(crbug.com/41483591): Integrate UsbChooserDialog with
        // ModalDialogManager.
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (modalDialogManager != null
                && (modalDialogManager.isSuspended(ModalDialogManager.ModalDialogType.TAB)
                        || modalDialogManager.isSuspended(
                                ModalDialogManager.ModalDialogType.APP))) {
            return null;
        }

        UsbChooserDialog dialog = new UsbChooserDialog(nativeUsbChooserDialogPtr, profile);
        dialog.show(activity, origin, securityLevel);
        return dialog;
    }

    @CalledByNative
    private void setIdleState() {
        mItemChooserDialog.setIdleState();
    }

    @VisibleForTesting
    @CalledByNative
    void addDevice(String deviceId, String deviceName) {
        mItemChooserDialog.addOrUpdateItem(deviceId, deviceName);
    }

    @CalledByNative
    private void removeDevice(String deviceId) {
        mItemChooserDialog.removeItemFromList(deviceId);
    }

    @CalledByNative
    private void closeDialog() {
        mNativeUsbChooserDialogPtr = 0;
        mItemChooserDialog.dismiss();
    }

    @NativeMethods
    interface Natives {
        void onItemSelected(long nativeUsbChooserDialogAndroid, String deviceId);

        void onDialogCancelled(long nativeUsbChooserDialogAndroid);

        void loadUsbHelpPage(long nativeUsbChooserDialogAndroid);
    }
}
