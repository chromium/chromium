// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.content_public.browser.bluetooth_scanning.Event;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * A dialog for asking user permission to do Bluetooth scanning. This dialog is shown when a
 * website requests to scan nearby Bluetooth devices (e.g. through a bluetooth.requestLEScan
 * Javascript call).
 *
 * The dialog is shown by create(), and always runs finishDialog() as it's closing.
 */
public class BluetoothScanningPermissionDialog {
    // How much of the height of the screen should be taken up by the listview.
    private static final float LISTVIEW_HEIGHT_PERCENT = 0.30f;
    // The height of a row of the listview in dp.
    private static final int LIST_ROW_HEIGHT_DP = 48;
    // The minimum height of the listview in the dialog (in dp).
    private static final int MIN_HEIGHT_DP = (int) (LIST_ROW_HEIGHT_DP * 1.5);
    // The maximum height of the listview in the dialog (in dp).
    private static final int MAX_HEIGHT_DP = (int) (LIST_ROW_HEIGHT_DP * 8.5);

    // The window that owns this dialog.
    private final WindowAndroid mWindowAndroid;

    // Always equal to mWindowAndroid.getActivity().get(), but stored separately to make sure it's
    // not GC'ed.
    private final Activity mActivity;

    // The dialog this class encapsulates.
    private Dialog mDialog;

    // Individual UI elements.
    private ListView mListView;

    // The adapter containing the items to show in the dialog.
    private DeviceItemAdapter mItemAdapter;

    // If this variable is false, the window should be closed when it loses focus;
    // Otherwise, the window should not be closed when it loses focus.
    private boolean mIgnorePendingWindowFocusChangeForClose;

    // A pointer back to the native part of the implementation for this dialog.
    private long mNativeBluetoothScanningPermissionDialogPtr;

    /**
     * Creates the BluetoothScanningPermissionDialog.
     *
     * @param windowAndroid The window that owns this dialog.
     * @param origin The origin for the site wanting to do Bluetooth scanning.
     * @param securityLevel The security level of the connection to the site wanting to do
     *                      Bluetooth scanning. For valid values see
     *                      SecurityStateModel::SecurityLevel.
     * @param nativeBluetoothScanningPermissionDialogPtr A pointer back to the native part of the
     *                                                   implementation for this dialog.
     */
    @VisibleForTesting
    BluetoothScanningPermissionDialog(WindowAndroid windowAndroid, String origin, int securityLevel,
            long nativeBluetoothScanningPermissionDialogPtr) {
        mWindowAndroid = windowAndroid;
        mActivity = windowAndroid.getActivity().get();
        assert mActivity != null;
        mNativeBluetoothScanningPermissionDialogPtr = nativeBluetoothScanningPermissionDialogPtr;

        // Emphasize the origin.
        Profile profile = Profile.getLastUsedProfile();
        SpannableString originSpannableString = new SpannableString(origin);

        assert mActivity instanceof ChromeBaseAppCompatActivity;
        final boolean useDarkColors = !((ChromeBaseAppCompatActivity) mActivity)
                                               .getNightModeStateProvider()
                                               .isInNightMode();

        OmniboxUrlEmphasizer.emphasizeUrl(originSpannableString, mActivity.getResources(), profile,
                securityLevel, /*isInternalPage=*/false, useDarkColors,
                /*emphasizeScheme=*/true);
        // Construct a full string and replace the |originSpannableString| text with emphasized
        // version.
        SpannableString title = new SpannableString(
                mActivity.getString(R.string.bluetooth_scanning_prompt_origin, origin));
        int start = title.toString().indexOf(origin);
        TextUtils.copySpansFrom(originSpannableString, 0, originSpannableString.length(),
                Object.class, title, start);

        String noneFound =
                mActivity.getString(R.string.bluetooth_scanning_prompt_no_devices_found_prompt);
        String blockButtonText =
                mActivity.getString(R.string.bluetooth_scanning_prompt_block_button_text);
        String allowButtonText =
                mActivity.getString(R.string.bluetooth_scanning_prompt_allow_button_text);

        LinearLayout dialogContainer = (LinearLayout) LayoutInflater.from(mActivity).inflate(
                R.layout.bluetooth_scanning_permission_dialog, null);

        TextViewWithClickableSpans dialogTitle =
                (TextViewWithClickableSpans) dialogContainer.findViewById(R.id.dialog_title);
        dialogTitle.setText(title);
        dialogTitle.setMovementMethod(LinkMovementMethod.getInstance());

        TextViewWithClickableSpans emptyMessage =
                (TextViewWithClickableSpans) dialogContainer.findViewById(R.id.not_found_message);
        emptyMessage.setText(noneFound);
        emptyMessage.setMovementMethod(LinkMovementMethod.getInstance());
        emptyMessage.setVisibility(View.VISIBLE);

        mListView = (ListView) dialogContainer.findViewById(R.id.items);
        mItemAdapter = new DeviceItemAdapter(mActivity, /*itemsSelectable=*/false,
                R.layout.bluetooth_scanning_permission_dialog_row);
        mItemAdapter.setNotifyOnChange(true);
        mListView.setAdapter(mItemAdapter);
        mListView.setEmptyView(emptyMessage);
        mListView.setDivider(null);

        ProgressBar progressBar = (ProgressBar) dialogContainer.findViewById(R.id.progress);
        progressBar.setVisibility(View.GONE);

        Button blockButton = (Button) dialogContainer.findViewById(R.id.block);
        blockButton.setText(blockButtonText);
        blockButton.setEnabled(true);
        blockButton.setOnClickListener(v -> {
            finishDialog(Event.BLOCK);
            mDialog.setOnDismissListener(null);
            mDialog.dismiss();
        });

        Button allowButton = (Button) dialogContainer.findViewById(R.id.allow);
        allowButton.setText(allowButtonText);
        allowButton.setEnabled(true);
        allowButton.setOnClickListener(v -> {
            finishDialog(Event.ALLOW);
            mDialog.setOnDismissListener(null);
            mDialog.dismiss();
        });

        mIgnorePendingWindowFocusChangeForClose = false;

        showDialogForView(dialogContainer);

        dialogContainer.addOnLayoutChangeListener(
                (View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) -> {
                    if (l != ol || t != ot || r != or || b != ob) {
                        // The list is the main element in the dialog and it should grow and
                        // shrink according to the size of the screen available.
                        View listViewContainer = dialogContainer.findViewById(R.id.container);
                        listViewContainer.setLayoutParams(new LinearLayout.LayoutParams(
                                LayoutParams.MATCH_PARENT,
                                getListHeight(mActivity.getWindow().getDecorView().getHeight(),
                                        mActivity.getResources().getDisplayMetrics().density)));
                    }
                });
    }

    @CalledByNative
    private static BluetoothScanningPermissionDialog create(WindowAndroid windowAndroid,
            String origin, int securityLevel, long nativeBluetoothScanningPermissionDialogPtr) {
        BluetoothScanningPermissionDialog dialog = new BluetoothScanningPermissionDialog(
                windowAndroid, origin, securityLevel, nativeBluetoothScanningPermissionDialogPtr);
        return dialog;
    }

    @VisibleForTesting
    @CalledByNative
    void addOrUpdateDevice(String deviceId, String deviceName) {
        if (TextUtils.isEmpty(deviceName)) {
            deviceName = mActivity.getString(R.string.bluetooth_scanning_device_unknown, deviceId);
        }
        mItemAdapter.addOrUpdate(deviceId, deviceName, /*icon=*/null, /*iconDescription=*/null);
        mListView.setVisibility(View.VISIBLE);
    }

    @CalledByNative
    private void closeDialog() {
        mNativeBluetoothScanningPermissionDialogPtr = 0;
        mDialog.dismiss();
    }

    // Computes the height of the device list, bound to half-multiples of the
    // row height so that it's obvious if there are more elements to scroll to.
    static int getListHeight(int decorHeight, float density) {
        float heightDp = decorHeight / density * LISTVIEW_HEIGHT_PERCENT;
        // Round to (an integer + 0.5) times LIST_ROW_HEIGHT.
        heightDp = (Math.round(heightDp / LIST_ROW_HEIGHT_DP - 0.5f) + 0.5f) * LIST_ROW_HEIGHT_DP;
        heightDp = MathUtils.clamp(heightDp, MIN_HEIGHT_DP, MAX_HEIGHT_DP);
        return Math.round(heightDp * density);
    }

    private void showDialogForView(View view) {
        mDialog = new Dialog(mActivity) {
            @Override
            public void onWindowFocusChanged(boolean hasFocus) {
                super.onWindowFocusChanged(hasFocus);
                if (!mIgnorePendingWindowFocusChangeForClose && !hasFocus) super.dismiss();
                mIgnorePendingWindowFocusChangeForClose = false;
            }
        };
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.setCanceledOnTouchOutside(true);
        mDialog.addContentView(view,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));
        mDialog.setOnCancelListener(dialog -> finishDialog(Event.CANCELED));

        Window window = mDialog.getWindow();
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            // On smaller screens, make the dialog fill the width of the screen,
            // and appear at the top.
            window.setBackgroundDrawable(new ColorDrawable(Color.WHITE));
            window.setGravity(Gravity.TOP);
            window.setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        mDialog.show();
    }

    // Called to report the permission dialog's results back to native code.
    private void finishDialog(int resultCode) {
        if (mNativeBluetoothScanningPermissionDialogPtr == 0) return;
        Natives jni = BluetoothScanningPermissionDialogJni.get();
        jni.onDialogFinished(mNativeBluetoothScanningPermissionDialogPtr, resultCode);
    }

    /**
     * Returns the dialog associated with this class. For use with tests only.
     */
    @VisibleForTesting
    public Dialog getDialogForTesting() {
        return mDialog;
    }

    /**
     * Returns the ItemAdapter associated with this class. For use with tests only.
     */
    @VisibleForTesting
    public DeviceItemAdapter getItemAdapterForTesting() {
        return mItemAdapter;
    }

    @NativeMethods
    interface Natives {
        void onDialogFinished(long nativeBluetoothScanningPromptAndroid, int eventType);
    }
}
