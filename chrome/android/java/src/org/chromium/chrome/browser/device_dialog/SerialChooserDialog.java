// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.text.SpannableString;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.permissions.ItemChooserDialog;
import org.chromium.components.permissions.PermissionUtil;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A dialog for showing available serial devices. This dialog is shown when a website requests to
 * connect to a serial device (e.g. through a serial.requestPort Javascript call).
 */
@NullMarked
public class SerialChooserDialog
        implements ItemChooserDialog.ItemSelectedCallback, PermissionCallback {

    /** The window that owns this dialog. */
    private final WindowAndroid mWindowAndroid;

    /**
     * Always equals to mWindow.getContext().get(), but stored separately to make sure it's not
     * GC'ed.
     */
    private final Context mContext;

    /** The dialog to show to let the user pick a device. */
    @VisibleForTesting ItemChooserDialog mItemChooserDialog;

    /** A pointer back to the native part of the implementation for this dialog. */
    private long mNativeSerialChooserDialogPtr;

    /** The current profile when the dialog is created. */
    private final Profile mProfile;

    /** Help text to show when the Bluetooth adapter is off. */
    private @Nullable SpannableString mAdapterOffStatus;

    // Used to keep track of when the Mode Changed Receiver is registered.
    boolean mIsLocationModeChangedReceiverRegistered;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    final BroadcastReceiver mLocationModeBroadcastReceiver =
            new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (!LocationManager.MODE_CHANGED_ACTION.equals(intent.getAction())) return;

                    mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(false);

                    if (!updateDialogWithBluetoothPermissionsAndLocationServices()) return;

                    mItemChooserDialog.clear();

                    Natives jni = SerialChooserDialogJni.get();
                    jni.listDevices(mNativeSerialChooserDialogPtr);
                }
            };

    // The type of link that is shown within the dialog.
    @IntDef({
        LinkType.ADAPTER_OFF,
        LinkType.ADAPTER_OFF_HELP,
        LinkType.REQUEST_PERMISSIONS,
        LinkType.REQUEST_LOCATION_SERVICES,
        LinkType.NEED_PERMISSION_HELP,
        LinkType.NEED_LOCATION_PERMISSION_HELP
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LinkType {
        int ADAPTER_OFF = 0;
        int ADAPTER_OFF_HELP = 1;
        int REQUEST_PERMISSIONS = 2;
        int REQUEST_LOCATION_SERVICES = 3;
        int NEED_PERMISSION_HELP = 4;
        int NEED_LOCATION_PERMISSION_HELP = 5;
    }

    /** Creates the SerialChooserDialog. */
    @VisibleForTesting
    SerialChooserDialog(
            WindowAndroid windowAndroid, long nativeSerialChooserDialogPtr, Profile profile) {
        mWindowAndroid = windowAndroid;
        mContext = assertNonNull(mWindowAndroid.getContext().get());
        mNativeSerialChooserDialogPtr = nativeSerialChooserDialogPtr;
        mProfile = profile;
    }

    /**
     * Shows the SerialChooserDialog.
     *
     * @param activity Activity which is used for launching a dialog.
     * @param origin The origin for the site wanting to connect to the serial port.
     * @param securityLevel The security level of the connection to the site wanting to connect to
     *     the serial port. For valid values see SecurityStateModel::SecurityLevel.
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
                new SpannableString(
                        activity.getString(R.string.serial_chooser_dialog_prompt, origin));
        int start = title.toString().indexOf(origin);
        TextUtils.copySpansFrom(
                originSpannableString,
                0,
                originSpannableString.length(),
                Object.class,
                title,
                start);

        String searching = "";
        SpannableString statusActive =
                SpanApplier.applySpans(
                        activity.getString(R.string.serial_chooser_dialog_footnote_text),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        activity,
                                        (view) -> {
                                            if (mNativeSerialChooserDialogPtr == 0) return;

                                            Natives jni = SerialChooserDialogJni.get();
                                            jni.openSerialHelpPage(mNativeSerialChooserDialogPtr);

                                            // Get rid of the highlight background on selection.
                                            view.invalidate();
                                        })));
        SpannableString statusIdleNoneFound = statusActive;
        SpannableString statusIdleSomeFound = statusActive;

        ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(
                        title,
                        searching,
                        activity.getString(R.string.serial_chooser_dialog_no_devices_found_prompt),
                        statusActive,
                        statusIdleNoneFound,
                        statusIdleSomeFound,
                        activity.getString(R.string.serial_chooser_dialog_connect_button_text));
        mItemChooserDialog = new ItemChooserDialog(activity, activity.getWindow(), this, labels);

        ContextUtils.registerProtectedBroadcastReceiver(
                mContext,
                mLocationModeBroadcastReceiver,
                new IntentFilter(LocationManager.MODE_CHANGED_ACTION));
        mIsLocationModeChangedReceiverRegistered = true;
    }

    @Override
    public void onItemSelected(String id) {
        if (mNativeSerialChooserDialogPtr != 0) {
            Natives jni = SerialChooserDialogJni.get();
            if (id.isEmpty()) {
                jni.onDialogCancelled(mNativeSerialChooserDialogPtr);
            } else {
                jni.onItemSelected(mNativeSerialChooserDialogPtr, id);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] results) {
        if (mNativeSerialChooserDialogPtr == 0) return;

        mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(false);

        if (!updateDialogWithBluetoothPermissionsAndLocationServices()) return;

        mItemChooserDialog.clear();
        SerialChooserDialogJni.get().listDevices(mNativeSerialChooserDialogPtr);
    }

    /** Returns true if Chrome has Bluetooth system permissions and the location service is on. */
    private boolean updateDialogWithBluetoothPermissionsAndLocationServices() {
        final boolean hasPermission =
                PermissionUtil.hasSystemPermissionsForBluetooth(mWindowAndroid);
        final boolean needsLocationServices = PermissionUtil.needsLocationServicesForBluetooth();

        if (!hasPermission
                && !PermissionUtil.canRequestSystemPermissionsForBluetooth(mWindowAndroid)) {
            // Immediately close the dialog because the user has asked Chrome not to request the
            // necessary permissions.
            finishDialog();
            return false;
        }

        // Compute the message to show the user.
        final SpanInfo servicesSpan =
                new SpanInfo(
                        "<services_link>",
                        "</services_link>",
                        createLinkSpan(LinkType.REQUEST_LOCATION_SERVICES));
        final SpannableString needPermissionMessage;
        if (hasPermission) {
            if (needsLocationServices) {
                needPermissionMessage =
                        SpanApplier.applySpans(
                                mContext.getString(R.string.bluetooth_need_location_services_on),
                                servicesSpan);
            } else {
                // We don't need to request anything.
                return true;
            }
        } else {
            final SpanInfo permissionSpan =
                    new SpanInfo(
                            "<permission_link>",
                            "</permission_link>",
                            createLinkSpan(LinkType.REQUEST_PERMISSIONS));
            if (needsLocationServices) {
                // If it needs locations services, it implicitly means it is a version
                // lower than Android S, so we can assume the system permission
                // needed is location permission.
                int resourceId = R.string.bluetooth_need_location_permission_and_services_on;
                needPermissionMessage =
                        SpanApplier.applySpans(
                                mContext.getString(resourceId), permissionSpan, servicesSpan);
            } else {
                if (PermissionUtil.needsNearbyDevicesPermissionForBluetooth(mWindowAndroid)) {
                    int resourceId = R.string.bluetooth_need_nearby_devices_permission;
                    needPermissionMessage =
                            SpanApplier.applySpans(mContext.getString(resourceId), permissionSpan);
                } else {
                    int resourceId = R.string.bluetooth_need_location_permission;
                    needPermissionMessage =
                            SpanApplier.applySpans(mContext.getString(resourceId), permissionSpan);
                }
            }
        }

        SpannableString needPermissionStatus =
                SpanApplier.applySpans(
                        mContext.getString(R.string.bluetooth_need_location_permission_help),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                createLinkSpan(LinkType.NEED_LOCATION_PERMISSION_HELP)));

        mItemChooserDialog.setErrorState(needPermissionMessage, needPermissionStatus);
        return false;
    }

    private ChromeClickableSpan createLinkSpan(@LinkType int linkType) {
        return new ChromeClickableSpan(mContext, (view) -> onLinkClick(view, linkType));
    }

    private void onLinkClick(View view, @LinkType int linkType) {
        if (mNativeSerialChooserDialogPtr == 0) return;

        Natives jni = SerialChooserDialogJni.get();

        switch (linkType) {
            case LinkType.ADAPTER_OFF:
                {
                    BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                    if (adapter != null && adapter.enable()) {
                        mItemChooserDialog.signalInitializingAdapter();
                    } else {
                        String unableToTurnOnAdapter =
                                mContext.getString(R.string.bluetooth_unable_to_turn_on_adapter);
                        mItemChooserDialog.setErrorState(
                                unableToTurnOnAdapter, assertNonNull(mAdapterOffStatus));
                    }
                    break;
                }
            case LinkType.ADAPTER_OFF_HELP:
                jni.openAdapterOffHelpPage(mNativeSerialChooserDialogPtr);
                break;
            case LinkType.NEED_PERMISSION_HELP:
            case LinkType.NEED_LOCATION_PERMISSION_HELP:
                jni.openBluetoothPermissionHelpPage(mNativeSerialChooserDialogPtr);
                break;
            case LinkType.REQUEST_PERMISSIONS:
                mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(true);
                PermissionUtil.requestSystemPermissionsForBluetooth(mWindowAndroid, this);
                break;
            case LinkType.REQUEST_LOCATION_SERVICES:
                mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(true);
                PermissionUtil.requestLocationServices(mWindowAndroid);
                break;
            default:
                assert false;
        }

        // Get rid of the highlight background on selection.
        view.invalidate();
    }

    @CalledByNative
    @VisibleForTesting
    static @Nullable SerialChooserDialog create(
            WindowAndroid windowAndroid,
            @JniType("std::u16string") String origin,
            int securityLevel,
            Profile profile,
            long nativeSerialChooserDialogPtr) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;

        // Avoid showing the chooser when ModalDialogManager indicates that
        // tab-modal or app-modal dialogs are suspended.
        // TODO(crbug.com/41483591): Integrate SerialChooserDialog with
        // ModalDialogManager.
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (modalDialogManager != null
                && (modalDialogManager.isSuspended(ModalDialogManager.ModalDialogType.TAB)
                        || modalDialogManager.isSuspended(
                                ModalDialogManager.ModalDialogType.APP))) {
            return null;
        }

        SerialChooserDialog dialog =
                new SerialChooserDialog(windowAndroid, nativeSerialChooserDialogPtr, profile);
        dialog.show(activity, origin, securityLevel);
        return dialog;
    }

    @CalledByNative
    private void setIdleState() {
        mItemChooserDialog.setIdleState();
    }

    @VisibleForTesting
    @CalledByNative
    void addDevice(
            @JniType("std::string") String deviceId, @JniType("std::u16string") String deviceName) {
        mItemChooserDialog.addOrUpdateItem(deviceId, deviceName);
    }

    @CalledByNative
    private void removeDevice(@JniType("std::string") String deviceId) {
        mItemChooserDialog.removeItemFromList(deviceId);
    }

    @VisibleForTesting
    @CalledByNative
    void closeDialog() {
        mNativeSerialChooserDialogPtr = 0;
        mItemChooserDialog.dismiss();
    }

    @VisibleForTesting
    @CalledByNative
    void onAdapterAuthorizationChanged(boolean authorized) {
        if (authorized) {
            mItemChooserDialog.clear();
            return;
        }
        updateDialogWithBluetoothPermissionsAndLocationServices();
    }

    @VisibleForTesting
    @CalledByNative
    void onAdapterEnabledChanged(boolean enabled) {
        if (enabled) {
            mItemChooserDialog.clear();
            return;
        }

        // Permission is required to turn the adapter on so make sure to ask for that first.
        if (updateDialogWithBluetoothPermissionsAndLocationServices()) {
            SpannableString adapterOffMessage =
                    SpanApplier.applySpans(
                            mContext.getString(R.string.bluetooth_adapter_off),
                            new SpanInfo(
                                    "<link>", "</link>", createLinkSpan(LinkType.ADAPTER_OFF)));

            mAdapterOffStatus =
                    SpanApplier.applySpans(
                            mContext.getString(R.string.bluetooth_adapter_off_help),
                            new SpanInfo(
                                    "<link>",
                                    "</link>",
                                    createLinkSpan(LinkType.ADAPTER_OFF_HELP)));
            mItemChooserDialog.setErrorState(adapterOffMessage, mAdapterOffStatus);
        }
    }

    private void finishDialog() {
        if (mIsLocationModeChangedReceiverRegistered) {
            mContext.unregisterReceiver(mLocationModeBroadcastReceiver);
            mIsLocationModeChangedReceiverRegistered = false;
        }

        mItemChooserDialog.dismiss();
        SerialChooserDialogJni.get().onDialogCancelled(mNativeSerialChooserDialogPtr);
    }

    @NativeMethods
    interface Natives {
        void listDevices(long nativeSerialChooserDialogAndroid);

        void onItemSelected(
                long nativeSerialChooserDialogAndroid, @JniType("std::string") String deviceId);

        void onDialogCancelled(long nativeSerialChooserDialogAndroid);

        void openSerialHelpPage(long nativeSerialChooserDialogAndroid);

        void openAdapterOffHelpPage(long nativeSerialChooserDialogAndroid);

        void openBluetoothPermissionHelpPage(long nativeSerialChooserDialogAndroid);
    }
}
