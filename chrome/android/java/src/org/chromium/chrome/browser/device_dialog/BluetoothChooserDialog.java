// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.location.LocationManager;
import android.text.SpannableString;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.content_public.browser.bluetooth.BluetoothChooserEvent;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A dialog for picking available Bluetooth devices. This dialog is shown when a website requests to
 * pair with a certain class of Bluetooth devices (e.g. through a bluetooth.requestDevice Javascript
 * call).
 *
 * The dialog is shown by create() or show(), and always runs finishDialog() as it's closing.
 */
public class BluetoothChooserDialog
        implements ItemChooserDialog.ItemSelectedCallback, PermissionCallback {
    private static final String TAG = "Bluetooth";

    // These constants match BluetoothChooserAndroid::ShowDiscoveryState, and are used in
    // notifyDiscoveryState().
    @IntDef({DiscoveryMode.DISCOVERY_FAILED_TO_START, DiscoveryMode.DISCOVERING,
            DiscoveryMode.DISCOVERY_IDLE})
    @Retention(RetentionPolicy.SOURCE)
    @interface DiscoveryMode {
        int DISCOVERY_FAILED_TO_START = 0;
        int DISCOVERING = 1;
        int DISCOVERY_IDLE = 2;
    }

    // The window that owns this dialog.
    final WindowAndroid mWindowAndroid;

    // Always equal to mWindowAndroid.getActivity().get(), but stored separately to make sure it's
    // not GC'ed.
    final Activity mActivity;

    // The dialog to show to let the user pick a device.
    ItemChooserDialog mItemChooserDialog;

    // The origin for the site wanting to pair with the bluetooth devices.
    String mOrigin;

    // The security level of the connection to the site wanting to pair with the
    // bluetooth devices. For valid values see SecurityStateModel::SecurityLevel.
    int mSecurityLevel;

    @VisibleForTesting
    Drawable mConnectedIcon;
    @VisibleForTesting
    String mConnectedIconDescription;
    @VisibleForTesting
    Drawable[] mSignalStrengthLevelIcon;

    // A pointer back to the native part of the implementation for this dialog.
    long mNativeBluetoothChooserDialogPtr;

    // Used to keep track of when the Mode Changed Receiver is registered.
    boolean mIsLocationModeChangedReceiverRegistered;

    // The local device Bluetooth adapter.
    private final BluetoothAdapter mAdapter;

    // The status message to show when the bluetooth adapter is turned off.
    private final SpannableString mAdapterOffStatus;

    @VisibleForTesting
    final BroadcastReceiver mLocationModeBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!LocationManager.MODE_CHANGED_ACTION.equals(intent.getAction())) {
                return;
            }
            if (checkLocationServicesAndPermission()) {
                mItemChooserDialog.clear();
                Natives jni = BluetoothChooserDialogJni.get();
                jni.restartSearch(mNativeBluetoothChooserDialogPtr);
            }
        }
    };

    // The type of link that is shown within the dialog.
    @IntDef({LinkType.EXPLAIN_BLUETOOTH, LinkType.ADAPTER_OFF, LinkType.ADAPTER_OFF_HELP,
            LinkType.REQUEST_LOCATION_PERMISSION, LinkType.REQUEST_LOCATION_SERVICES,
            LinkType.NEED_LOCATION_PERMISSION_HELP, LinkType.RESTART_SEARCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LinkType {
        int EXPLAIN_BLUETOOTH = 0;
        int ADAPTER_OFF = 1;
        int ADAPTER_OFF_HELP = 2;
        int REQUEST_LOCATION_PERMISSION = 3;
        int REQUEST_LOCATION_SERVICES = 4;
        int NEED_LOCATION_PERMISSION_HELP = 5;
        int RESTART_SEARCH = 6;
    }

    /**
     * Creates the BluetoothChooserDialog.
     */
    @VisibleForTesting
    BluetoothChooserDialog(WindowAndroid windowAndroid, String origin, int securityLevel,
            long nativeBluetoothChooserDialogPtr) {
        mWindowAndroid = windowAndroid;
        mActivity = windowAndroid.getActivity().get();
        assert mActivity != null;
        mOrigin = origin;
        mSecurityLevel = securityLevel;
        mNativeBluetoothChooserDialogPtr = nativeBluetoothChooserDialogPtr;
        mAdapter = BluetoothAdapter.getDefaultAdapter();

        // Initialize icons.
        mConnectedIcon = getIconWithRowIconColorStateList(R.drawable.ic_bluetooth_connected);
        mConnectedIconDescription = mActivity.getString(R.string.bluetooth_device_connected);

        mSignalStrengthLevelIcon = new Drawable[] {
                getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_0_bar),
                getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_1_bar),
                getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_2_bar),
                getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_3_bar),
                getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_4_bar)};

        if (mAdapter == null) {
            Log.i(TAG, "BluetoothChooserDialog: Default Bluetooth adapter not found.");
        }
        mAdapterOffStatus = SpanApplier.applySpans(
                mActivity.getString(R.string.bluetooth_adapter_off_help),
                new SpanInfo("<link>", "</link>", createLinkSpan(LinkType.ADAPTER_OFF_HELP)));
    }

    private Drawable getIconWithRowIconColorStateList(int icon) {
        Resources res = mActivity.getResources();

        Drawable drawable = VectorDrawableCompat.create(res, icon, mActivity.getTheme());
        DrawableCompat.setTintList(drawable,
                AppCompatResources.getColorStateList(
                        mActivity, R.color.item_chooser_row_icon_color));
        return drawable;
    }

    /**
     * Show the BluetoothChooserDialog.
     */
    @VisibleForTesting
    void show() {
        // Emphasize the origin.
        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        Profile profile = Profile.getLastUsedRegularProfile();
        SpannableString origin = new SpannableString(mOrigin);

        final boolean useDarkColors = !ColorUtils.inNightMode(mActivity);
        ChromeAutocompleteSchemeClassifier chromeAutocompleteSchemeClassifier =
                new ChromeAutocompleteSchemeClassifier(profile);

        OmniboxUrlEmphasizer.emphasizeUrl(origin, mActivity.getResources(),
                chromeAutocompleteSchemeClassifier, mSecurityLevel, false, useDarkColors, true);
        chromeAutocompleteSchemeClassifier.destroy();
        // Construct a full string and replace the origin text with emphasized version.
        SpannableString title =
                new SpannableString(mActivity.getString(R.string.bluetooth_dialog_title, mOrigin));
        int start = title.toString().indexOf(mOrigin);
        TextUtils.copySpansFrom(origin, 0, origin.length(), Object.class, title, start);

        String noneFound = mActivity.getString(R.string.bluetooth_not_found);

        SpannableString searching = SpanApplier.applySpans(
                mActivity.getString(R.string.bluetooth_searching),
                new SpanInfo("<link>", "</link>", createLinkSpan(LinkType.EXPLAIN_BLUETOOTH)));

        String positiveButton = mActivity.getString(R.string.bluetooth_confirm_button);

        SpannableString statusIdleNoneFound = SpanApplier.applySpans(
                mActivity.getString(R.string.bluetooth_not_seeing_it_idle),
                new SpanInfo("<link1>", "</link1>", createLinkSpan(LinkType.EXPLAIN_BLUETOOTH)),
                new SpanInfo("<link2>", "</link2>", createLinkSpan(LinkType.RESTART_SEARCH)));

        SpannableString statusActive = searching;

        SpannableString statusIdleSomeFound = statusIdleNoneFound;

        ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(title, searching, noneFound, statusActive,
                        statusIdleNoneFound, statusIdleSomeFound, positiveButton);
        mItemChooserDialog = new ItemChooserDialog(mActivity, this, labels);

        mActivity.registerReceiver(mLocationModeBroadcastReceiver,
                new IntentFilter(LocationManager.MODE_CHANGED_ACTION));
        mIsLocationModeChangedReceiverRegistered = true;
    }

    // Called to report the dialog's results back to native code.
    private void finishDialog(int resultCode, String id) {
        if (mIsLocationModeChangedReceiverRegistered) {
            mActivity.unregisterReceiver(mLocationModeBroadcastReceiver);
            mIsLocationModeChangedReceiverRegistered = false;
        }

        if (mNativeBluetoothChooserDialogPtr != 0) {
            Natives jni = BluetoothChooserDialogJni.get();
            jni.onDialogFinished(mNativeBluetoothChooserDialogPtr, resultCode, id);
        }
    }

    @Override
    public void onItemSelected(String id) {
        if (id.isEmpty()) {
            finishDialog(BluetoothChooserEvent.CANCELLED, "");
        } else {
            finishDialog(BluetoothChooserEvent.SELECTED, id);
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        // The chooser might have been closed during the request.
        if (mNativeBluetoothChooserDialogPtr == 0) return;

        for (int i = 0; i < permissions.length; i++) {
            if (permissions[i].equals(Manifest.permission.ACCESS_FINE_LOCATION)) {
                if (checkLocationServicesAndPermission()) {
                    mItemChooserDialog.clear();
                    Natives jni = BluetoothChooserDialogJni.get();
                    jni.restartSearch(mNativeBluetoothChooserDialogPtr);
                }
                return;
            }
        }
        // If the location permission is not present, leave the currently-shown message in place.
    }

    // Returns true if Location Services is on and Chrome has permission to see the user's location.
    private boolean checkLocationServicesAndPermission() {
        final boolean havePermission =
                mWindowAndroid.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION);
        final boolean locationServicesOn =
                LocationUtils.getInstance().isSystemLocationSettingEnabled();

        if (!havePermission
                && !mWindowAndroid.canRequestPermission(Manifest.permission.ACCESS_FINE_LOCATION)) {
            // Immediately close the dialog because the user has asked Chrome not to request the
            // location permission.
            finishDialog(BluetoothChooserEvent.DENIED_PERMISSION, "");
            return false;
        }

        // Compute the message to show the user.
        final SpanInfo permissionSpan = new SpanInfo("<permission_link>", "</permission_link>",
                createLinkSpan(LinkType.REQUEST_LOCATION_PERMISSION));
        final SpanInfo servicesSpan = new SpanInfo("<services_link>", "</services_link>",
                createLinkSpan(LinkType.REQUEST_LOCATION_SERVICES));
        final SpannableString needLocationMessage;
        if (havePermission) {
            if (locationServicesOn) {
                // We don't need to request anything.
                return true;
            } else {
                needLocationMessage = SpanApplier.applySpans(
                        mActivity.getString(R.string.bluetooth_need_location_services_on),
                        servicesSpan);
            }
        } else {
            if (locationServicesOn) {
                needLocationMessage = SpanApplier.applySpans(
                        mActivity.getString(R.string.bluetooth_need_location_permission),
                        permissionSpan);
            } else {
                needLocationMessage = SpanApplier.applySpans(
                        mActivity.getString(
                                R.string.bluetooth_need_location_permission_and_services_on),
                        permissionSpan, servicesSpan);
            }
        }

        SpannableString needLocationStatus = SpanApplier.applySpans(
                mActivity.getString(R.string.bluetooth_need_location_permission_help),
                new SpanInfo("<link>", "</link>",
                        createLinkSpan(LinkType.NEED_LOCATION_PERMISSION_HELP)));

        mItemChooserDialog.setErrorState(needLocationMessage, needLocationStatus);
        return false;
    }

    private NoUnderlineClickableSpan createLinkSpan(@LinkType int linkType) {
        return new NoUnderlineClickableSpan(
                mActivity.getResources(), (view) -> onBluetoothLinkClick(view, linkType));
    }

    private void onBluetoothLinkClick(View view, @LinkType int linkType) {
        if (mNativeBluetoothChooserDialogPtr == 0) return;

        Natives jni = BluetoothChooserDialogJni.get();

        switch (linkType) {
            case LinkType.EXPLAIN_BLUETOOTH:
                // No need to close the dialog here because
                // ShowBluetoothOverviewLink will close it.
                jni.showBluetoothOverviewLink(mNativeBluetoothChooserDialogPtr);
                break;
            case LinkType.ADAPTER_OFF:
                if (mAdapter != null && mAdapter.enable()) {
                    mItemChooserDialog.signalInitializingAdapter();
                } else {
                    String unableToTurnOnAdapter =
                            mActivity.getString(R.string.bluetooth_unable_to_turn_on_adapter);
                    mItemChooserDialog.setErrorState(unableToTurnOnAdapter, mAdapterOffStatus);
                }
                break;
            case LinkType.ADAPTER_OFF_HELP:
                jni.showBluetoothAdapterOffLink(mNativeBluetoothChooserDialogPtr);
                break;
            case LinkType.REQUEST_LOCATION_PERMISSION:
                mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(true);
                mWindowAndroid.requestPermissions(
                        new String[] {Manifest.permission.ACCESS_FINE_LOCATION},
                        BluetoothChooserDialog.this);
                break;
            case LinkType.REQUEST_LOCATION_SERVICES:
                mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(true);
                mActivity.startActivity(
                        LocationUtils.getInstance().getSystemLocationSettingsIntent());
                break;
            case LinkType.NEED_LOCATION_PERMISSION_HELP:
                jni.showNeedLocationPermissionLink(mNativeBluetoothChooserDialogPtr);
                break;
            case LinkType.RESTART_SEARCH:
                mItemChooserDialog.clear();
                jni.restartSearch(mNativeBluetoothChooserDialogPtr);
                break;
            default:
                assert false;
        }

        // Get rid of the highlight background on selection.
        view.invalidate();
    }

    @CalledByNative
    private static BluetoothChooserDialog create(WindowAndroid windowAndroid, String origin,
            int securityLevel, long nativeBluetoothChooserDialogPtr) {
        if (!windowAndroid.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                && !windowAndroid.canRequestPermission(Manifest.permission.ACCESS_FINE_LOCATION)) {
            // If we can't even ask for enough permission to scan for Bluetooth devices, don't open
            // the dialog.
            return null;
        }
        BluetoothChooserDialog dialog = new BluetoothChooserDialog(
                windowAndroid, origin, securityLevel, nativeBluetoothChooserDialogPtr);
        dialog.show();
        return dialog;
    }

    @VisibleForTesting
    @CalledByNative
    void addOrUpdateDevice(
            String deviceId, String deviceName, boolean isGATTConnected, int signalStrengthLevel) {
        Drawable icon = null;
        String iconDescription = null;
        if (isGATTConnected) {
            icon = mConnectedIcon.getConstantState().newDrawable();
            iconDescription = mConnectedIconDescription;
        } else if (signalStrengthLevel != -1) {
            icon = mSignalStrengthLevelIcon[signalStrengthLevel].getConstantState().newDrawable();
            iconDescription = mActivity.getResources().getQuantityString(
                    R.plurals.signal_strength_level_n_bars, signalStrengthLevel,
                    signalStrengthLevel);
        }

        mItemChooserDialog.addOrUpdateItem(deviceId, deviceName, icon, iconDescription);
    }

    @VisibleForTesting
    @CalledByNative
    void closeDialog() {
        mNativeBluetoothChooserDialogPtr = 0;
        mItemChooserDialog.dismiss();
    }

    @VisibleForTesting
    @CalledByNative
    void notifyAdapterTurnedOff() {
        SpannableString adapterOffMessage =
                SpanApplier.applySpans(mActivity.getString(R.string.bluetooth_adapter_off),
                        new SpanInfo("<link>", "</link>", createLinkSpan(LinkType.ADAPTER_OFF)));

        mItemChooserDialog.setErrorState(adapterOffMessage, mAdapterOffStatus);
    }

    @CalledByNative
    private void notifyAdapterTurnedOn() {
        mItemChooserDialog.clear();
    }

    @VisibleForTesting
    @CalledByNative
    void notifyDiscoveryState(@DiscoveryMode int discoveryState) {
        switch (discoveryState) {
            case DiscoveryMode.DISCOVERY_FAILED_TO_START: {
                // FAILED_TO_START might be caused by a missing Location
                // permission or by the Location service being turned off.
                // Check, and show a request if so.
                checkLocationServicesAndPermission();
                break;
            }
            case DiscoveryMode.DISCOVERY_IDLE: {
                mItemChooserDialog.setIdleState();
                break;
            }
            default: {
                // TODO(jyasskin): Report the new state to the user.
                break;
            }
        }
    }

    @NativeMethods
    interface Natives {
        void onDialogFinished(long nativeBluetoothChooserAndroid, int eventType, String deviceId);
        void restartSearch(long nativeBluetoothChooserAndroid);
        // Help links.
        void showBluetoothOverviewLink(long nativeBluetoothChooserAndroid);
        void showBluetoothAdapterOffLink(long nativeBluetoothChooserAndroid);
        void showNeedLocationPermissionLink(long nativeBluetoothChooserAndroid);
    }
}
