// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Activity;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * A dialog for showing available USB devices. This dialog is shown when a website requests to
 * connect to a USB device (e.g. through a usb.requestDevice Javascript call).
 */
public class UsbChooserDialog implements ItemChooserDialog.ItemSelectedCallback {
    /**
     * The dialog to show to let the user pick a device.
     */
    ItemChooserDialog mItemChooserDialog;

    /**
     * A pointer back to the native part of the implementation for this dialog.
     */
    long mNativeUsbChooserDialogPtr;

    /**
     * Creates the UsbChooserDialog.
     */
    @VisibleForTesting
    UsbChooserDialog(long nativeUsbChooserDialogPtr) {
        mNativeUsbChooserDialogPtr = nativeUsbChooserDialogPtr;
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
        Profile profile = Profile.getLastUsedProfile();
        SpannableString originSpannableString = new SpannableString(origin);

        assert activity instanceof ChromeBaseAppCompatActivity;
        final boolean useDarkColors = !((ChromeBaseAppCompatActivity) activity)
                                               .getNightModeStateProvider()
                                               .isInNightMode();

        OmniboxUrlEmphasizer.emphasizeUrl(originSpannableString, activity.getResources(), profile,
                securityLevel, false /* isInternalPage */, useDarkColors,
                true /* emphasizeHttpsScheme */);
        // Construct a full string and replace the origin text with emphasized version.
        SpannableString title =
                new SpannableString(activity.getString(R.string.usb_chooser_dialog_prompt, origin));
        int start = title.toString().indexOf(origin);
        TextUtils.copySpansFrom(originSpannableString, 0, originSpannableString.length(),
                Object.class, title, start);

        String searching = "";
        String noneFound = activity.getString(R.string.usb_chooser_dialog_no_devices_found_prompt);
        SpannableString statusActive = SpanApplier.applySpans(
                activity.getString(R.string.usb_chooser_dialog_footnote_text),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(activity.getResources(), (view) -> {
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
                new ItemChooserDialog.ItemChooserLabels(title, searching, noneFound, statusActive,
                        statusIdleNoneFound, statusIdleSomeFound, positiveButton);
        mItemChooserDialog = new ItemChooserDialog(activity, this, labels);
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
    private static UsbChooserDialog create(WindowAndroid windowAndroid, String origin,
            int securityLevel, long nativeUsbChooserDialogPtr) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;

        UsbChooserDialog dialog = new UsbChooserDialog(nativeUsbChooserDialogPtr);
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
