// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Activity;
import android.app.Dialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.os.Build;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.ItemChooserDialog;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.lang.ref.WeakReference;
import java.util.Arrays;

/** Tests for the SerialChooserDialog class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/344665244): Failing when batched, batch this again.
public class SerialChooserDialogTest {
    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private String mSelectedDeviceId = "";

    private ActivityWindowAndroid mWindowAndroid;
    private FakeLocationUtils mLocationUtils;
    private SerialChooserDialog mChooserDialog;

    private int mListDevicesCount;

    private class TestSerialChooserDialogJni implements SerialChooserDialog.Natives {
        @Override
        public void listDevices(long nativeSerialChooserDialogAndroid) {
            mListDevicesCount++;
        }

        @Override
        public void onItemSelected(long nativeSerialChooserDialogAndroid, String deviceId) {
            mSelectedDeviceId = deviceId;
        }

        @Override
        public void onDialogCancelled(long nativeSerialChooserDialogAndroid) {}

        @Override
        public void openSerialHelpPage(long nativeSerialChooserDialogAndroid) {}

        @Override
        public void openAdapterOffHelpPage(long nativeSerialChooserDialogAndroid) {}

        @Override
        public void openBluetoothPermissionHelpPage(long nativeSerialChooserDialogAndroid) {}
    }

    @Before
    public void setUp() throws Exception {
        mLocationUtils = new FakeLocationUtils();
        LocationUtils.setFactory(() -> mLocationUtils);
        SerialChooserDialogJni.setInstanceForTesting(new TestSerialChooserDialogJni());
        mChooserDialog = createDialog();
    }

    @After
    public void tearDown() {
        LocationUtils.setFactory(null);
    }

    private SerialChooserDialog createDialog() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
                    SerialChooserDialog dialog =
                            new SerialChooserDialog(
                                    mWindowAndroid,
                                    /* nativeSerialChooserDialogPtr= */ 42,
                                    ProfileManager.getLastUsedRegularProfile());
                    dialog.show(
                            mActivityTestRule.getActivity(),
                            "https://origin.example.com/",
                            ConnectionSecurityLevel.SECURE);
                    return dialog;
                });
    }

    private void selectItem(int position) {
        final Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        final ListView items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(items.getChildAt(0), Matchers.notNullValue()));

        // The actual index for the first item displayed on screen.
        int firstVisiblePosition = items.getFirstVisiblePosition();
        TouchCommon.singleClickView(items.getChildAt(position - firstVisiblePosition));

        CriteriaHelper.pollUiThread(() -> button.isEnabled());
        // Make sure the button is properly rendered before clicking.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(button.getHeight(), Matchers.greaterThan(0));
                });
        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mSelectedDeviceId, Matchers.not(Matchers.isEmptyString()));
                });
    }

    /**
     * The messages include <*link*> ... </*link*> sections that are used to create clickable spans.
     * For testing the messages, this function returns the raw string without the tags.
     */
    private static String removeLinkTags(String message) {
        return message.replaceAll("</?[^>]*link[^>]*>", "");
    }

    /**
     * The helper function help to determine whether |requestedPermissions| pass correct permissions
     * before and from Android S.
     */
    private static boolean checkRequestedPermissions(String[] requestedPermissions) {
        if (requestedPermissions == null) return false;
        String[] expectedPermissionBeforeS = {Manifest.permission.ACCESS_FINE_LOCATION};
        String[] expectedPermissionFromS = {
            Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN
        };
        String[] expected =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                        ? expectedPermissionFromS
                        : expectedPermissionBeforeS;
        String[] copied = Arrays.copyOf(requestedPermissions, requestedPermissions.length);
        Arrays.sort(expected);
        Arrays.sort(copied);
        return Arrays.equals(expected, copied);
    }

    @Test
    @SmallTest
    public void testCancel() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final ListView items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);

        // The 'Connect' button should be disabled and the list view should be hidden.
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        dialog.cancel();

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mSelectedDeviceId, Matchers.isEmptyString()));
    }

    @Test
    @SmallTest
    public void testSelectItem() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final ListView items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);
        final int position = 1;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mChooserDialog.addDevice("device_id_0", "device_name_0");
                    mChooserDialog.addDevice("device_id_1", "device_name_1");
                    mChooserDialog.addDevice("device_id_2", "device_name_2");
                    // Show the desired position at the top of the ListView (in case
                    // not all the items can fit on small devices' screens).
                    items.setSelection(position);
                });

        // After adding items to the dialog, the help message should be showing,
        // the 'Connect' button should still be disabled (since nothing's selected),
        // and the list view should show.
        Assert.assertEquals(
                removeLinkTags(
                        mActivityTestRule
                                .getActivity()
                                .getString(R.string.usb_chooser_dialog_footnote_text)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, items.getVisibility());

        selectItem(position);

        Assert.assertEquals("device_id_1", mSelectedDeviceId);
    }

    @Test
    @SmallTest
    public void testNoPermission() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView = dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mChooserDialog.onAdapterAuthorizationChanged(false));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            Assert.assertEquals(
                    removeLinkTags(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.bluetooth_need_nearby_devices_permission)),
                    errorView.getText().toString());
        } else {
            Assert.assertEquals(
                    removeLinkTags(
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.bluetooth_need_location_permission)),
                    errorView.getText().toString());
        }

        Assert.assertEquals(
                removeLinkTags(
                        mActivityTestRule
                                .getActivity()
                                .getString(R.string.bluetooth_adapter_off_help)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(
                () -> errorView.getClickableSpans()[0].onClick(errorView));

        // Permission was requested.
        Assert.assertTrue(checkRequestedPermissions(permissionDelegate.mPermissionsRequested));
        Assert.assertNotNull(permissionDelegate.mCallback);

        // Grant permission.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionDelegate.mBluetoothConnectGranted = true;
            permissionDelegate.mBluetoothScanGranted = true;
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            permissionDelegate.mCallback.onRequestPermissionsResult(
                                    new String[] {
                                        Manifest.permission.BLUETOOTH_CONNECT,
                                        Manifest.permission.BLUETOOTH_SCAN
                                    },
                                    new int[] {PackageManager.PERMISSION_GRANTED}));

        } else {
            permissionDelegate.mLocationGranted = true;
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            permissionDelegate.mCallback.onRequestPermissionsResult(
                                    new String[] {Manifest.permission.ACCESS_FINE_LOCATION},
                                    new int[] {PackageManager.PERMISSION_GRANTED}));
        }

        Assert.assertEquals(1, mListDevicesCount);
        mChooserDialog.closeDialog();
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(Build.VERSION_CODES.R)
    public void testNoLocationServices() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView = dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        // Grant permissions, and turn off location services.
        permissionDelegate.mLocationGranted = true;
        mLocationUtils.mSystemLocationSettingsEnabled = false;

        ThreadUtils.runOnUiThreadBlocking(
                () -> mChooserDialog.onAdapterAuthorizationChanged(false));

        Assert.assertEquals(
                removeLinkTags(
                        mActivityTestRule
                                .getActivity()
                                .getString(R.string.bluetooth_need_location_services_on)),
                errorView.getText().toString());
        Assert.assertEquals(
                removeLinkTags(
                        mActivityTestRule
                                .getActivity()
                                .getString(R.string.bluetooth_need_location_permission_help)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        // Turn on Location Services.
        mLocationUtils.mSystemLocationSettingsEnabled = true;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mChooserDialog.mLocationModeBroadcastReceiver.onReceive(
                                mActivityTestRule.getActivity(),
                                new Intent(LocationManager.MODE_CHANGED_ACTION)));

        Assert.assertEquals(1, mListDevicesCount);
        mChooserDialog.closeDialog();
    }

    // TODO(jyasskin): Test when the user denies Chrome the ability to ask for permission.

    @Test
    @SmallTest
    public void testTurnOnAdapter() {
        final ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView = dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        // Grant permission
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionDelegate.mBluetoothConnectGranted = true;
            permissionDelegate.mBluetoothScanGranted = true;
        } else {
            permissionDelegate.mLocationGranted = true;
        }

        // Turn off adapter.
        ThreadUtils.runOnUiThreadBlocking(() -> mChooserDialog.onAdapterEnabledChanged(false));

        Assert.assertEquals(
                removeLinkTags(
                        mActivityTestRule.getActivity().getString(R.string.bluetooth_adapter_off)),
                errorView.getText().toString());
        Assert.assertEquals(
                removeLinkTags(
                        mActivityTestRule
                                .getActivity()
                                .getString(R.string.bluetooth_adapter_off_help)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        // Turn on adapter.
        ThreadUtils.runOnUiThreadBlocking(() -> itemChooser.signalInitializingAdapter());

        Assert.assertEquals(View.GONE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.VISIBLE, progress.getVisibility());

        mChooserDialog.closeDialog();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/343347280")
    public void testChooserBlockedByModalDialogManager() {
        ModalDialogManager mockModalDialogManager = mock(ModalDialogManager.class);
        when(mockModalDialogManager.isSuspended(ModalDialogManager.ModalDialogType.APP))
                .thenReturn(true);
        when(mockModalDialogManager.isSuspended(ModalDialogManager.ModalDialogType.TAB))
                .thenReturn(true);
        Activity mockActivity = mock(Activity.class);
        WindowAndroid mockWindowAndroid = mock(WindowAndroid.class);
        when(mockWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockActivity));
        when(mockWindowAndroid.getModalDialogManager()).thenReturn(mockModalDialogManager);
        when(mockWindowAndroid.hasPermission(Manifest.permission.BLUETOOTH_SCAN)).thenReturn(true);
        when(mockWindowAndroid.hasPermission(Manifest.permission.BLUETOOTH_CONNECT))
                .thenReturn(true);
        when(mockWindowAndroid.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION))
                .thenReturn(true);

        SerialChooserDialog dialog;
        dialog =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return SerialChooserDialog.create(
                                    mockWindowAndroid,
                                    "https://origin.example.com/",
                                    ConnectionSecurityLevel.SECURE,
                                    ProfileManager.getLastUsedRegularProfile(),
                                    /* nativeSerialChooserDialogPtr= */ 42);
                        });
        Assert.assertNull(dialog);
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        final Dialog mDialog;
        PermissionCallback mCallback;
        String[] mPermissionsRequested;
        public boolean mLocationGranted;
        public boolean mBluetoothScanGranted;
        public boolean mBluetoothConnectGranted;

        public TestAndroidPermissionDelegate(Dialog dialog) {
            mLocationGranted = false;
            mBluetoothScanGranted = false;
            mBluetoothConnectGranted = false;
            mDialog = dialog;
        }

        @Override
        public boolean hasPermission(String permission) {
            if (permission.equals(Manifest.permission.ACCESS_FINE_LOCATION)) {
                return mLocationGranted;
            } else if (permission.equals(Manifest.permission.BLUETOOTH_SCAN)) {
                return mBluetoothScanGranted;
            } else if (permission.equals(Manifest.permission.BLUETOOTH_CONNECT)) {
                return mBluetoothConnectGranted;
            } else {
                return false;
            }
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return true;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
            // Requesting for permission takes away focus from the window.
            mDialog.onWindowFocusChanged(/* hasFocus= */ false);
            mPermissionsRequested = permissions;
            mCallback = callback;
        }

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

    private static class FakeLocationUtils extends LocationUtils {
        public boolean mLocationGranted;

        @Override
        public boolean hasAndroidLocationPermission() {
            return mLocationGranted;
        }

        public boolean mSystemLocationSettingsEnabled = true;

        @Override
        public boolean isSystemLocationSettingEnabled() {
            return mSystemLocationSettingsEnabled;
        }
    }
}
