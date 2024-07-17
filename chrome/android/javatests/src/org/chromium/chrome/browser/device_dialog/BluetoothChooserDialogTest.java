// Copyright 2016 The Chromium Authors
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
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.BluetoothChooserDialog;
import org.chromium.components.permissions.BluetoothChooserDialogJni;
import org.chromium.components.permissions.DeviceItemAdapter;
import org.chromium.components.permissions.ItemChooserDialog;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.bluetooth.BluetoothChooserEvent;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.concurrent.Callable;

/**
 * Tests for the BluetoothChooserDialog class.
 *
 * <p>TODO(crbug.com/40187298): Componentize this test.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/344665244): Failing when batched, batch this again.
public class BluetoothChooserDialogTest {
    public static final String DEVICE_DIALOG_BATCH_NAME = "device_dialog";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public JniMocker mocker = new JniMocker();

    private ActivityWindowAndroid mWindowAndroid;
    private FakeLocationUtils mLocationUtils;
    private BluetoothChooserDialog mChooserDialog;

    private int mFinishedEventType = -1;
    private String mFinishedDeviceId;
    private int mRestartSearchCount;

    // Unused member variables to avoid Java optimizer issues with Mockito.
    @Mock ModalDialogManager mMockModalDialogManager;
    @Mock Activity mMockActivity;
    @Mock WindowAndroid mMockWindowAndroid;

    private class TestBluetoothChooserDialogJni implements BluetoothChooserDialog.Natives {
        private BluetoothChooserDialog mBluetoothChooserDialog;

        TestBluetoothChooserDialogJni(BluetoothChooserDialog dialog) {
            mBluetoothChooserDialog = dialog;
        }

        @Override
        public void onDialogFinished(
                long nativeBluetoothChooserAndroid, int eventType, String deviceId) {
            Assert.assertEquals(
                    nativeBluetoothChooserAndroid,
                    mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr);
            Assert.assertEquals(mFinishedEventType, -1);
            mFinishedEventType = eventType;
            mFinishedDeviceId = deviceId;
            // The native code calls closeDialog() when OnDialogFinished is called.
            mBluetoothChooserDialog.closeDialog();
        }

        @Override
        public void restartSearch(long nativeBluetoothChooserAndroid) {
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
            mRestartSearchCount++;
        }

        @Override
        public void showBluetoothOverviewLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
        }

        @Override
        public void showBluetoothAdapterOffLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
        }

        @Override
        public void showNeedLocationPermissionLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
        }
    }

    @Before
    public void setUp() throws Exception {
        mLocationUtils = new FakeLocationUtils();
        LocationUtils.setFactory(() -> mLocationUtils);
        mChooserDialog = createDialog();
        mocker.mock(
                BluetoothChooserDialogJni.TEST_HOOKS,
                new TestBluetoothChooserDialogJni(mChooserDialog));
    }

    @After
    public void tearDown() {
        LocationUtils.setFactory(null);
    }

    private BluetoothChooserDialog createDialog() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid = sActivityTestRule.getActivity().getWindowAndroid();
                    BluetoothChooserDialog dialog =
                            new BluetoothChooserDialog(
                                    mWindowAndroid,
                                    "https://origin.example.com/",
                                    ConnectionSecurityLevel.SECURE,
                                    new ChromeBluetoothChooserAndroidDelegate(
                                            ProfileManager.getLastUsedRegularProfile()),
                                    /* nativeBluetoothChooserDialogPtr= */ 42);
                    dialog.show();
                    return dialog;
                });
    }

    private void selectItem(int position) {
        final Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        final ListView items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);

        clickItemAtPosition(items, position - 1);

        CriteriaHelper.pollUiThread(() -> button.isEnabled());
        // Make sure the button is properly rendered before clicking.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(button.getHeight(), Matchers.greaterThan(0));
                });

        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mFinishedEventType, Matchers.not(-1)));
    }

    private static void clickItemAtPosition(ListView listView, int position) {
        CriteriaHelper.pollUiThread(() -> listView.getChildAt(0) != null);

        Callable<Boolean> isVisible =
                () -> {
                    int visibleStart = listView.getFirstVisiblePosition();
                    int visibleEnd = visibleStart + listView.getChildCount() - 1;
                    return position >= visibleStart && position <= visibleEnd;
                };

        if (!ThreadUtils.runOnUiThreadBlocking(isVisible)) {
            ThreadUtils.runOnUiThreadBlocking(() -> listView.setSelection(position));
            CriteriaHelper.pollUiThread(isVisible);
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TouchCommon.singleClickView(
                            listView.getChildAt(position - listView.getFirstVisiblePosition()));
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
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final ListView items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule.getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        dialog.cancel();

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mFinishedEventType, Matchers.not(-1)));

        Assert.assertEquals(BluetoothChooserEvent.CANCELLED, mFinishedEventType);
        Assert.assertEquals("", mFinishedDeviceId);
    }

    @Test
    @SmallTest
    public void testDismiss() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final ListView items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule.getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        dialog.dismiss();

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mFinishedEventType, Matchers.not(-1)));

        Assert.assertEquals(BluetoothChooserEvent.CANCELLED, mFinishedEventType);
        Assert.assertEquals("", mFinishedDeviceId);
    }

    @Test
    @SmallTest
    public void testSelectItem() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = dialog.findViewById(R.id.status);
        final View items = dialog.findViewById(R.id.items);
        final Button button = dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Add non-connected device with no signal strength.
                    mChooserDialog.addOrUpdateDevice(
                            "id-1",
                            "Name 1",
                            /* isGATTConnected= */ false,
                            /* signalStrengthLevel= */ -1);
                    // Add connected device with no signal strength.
                    mChooserDialog.addOrUpdateDevice(
                            "id-2",
                            "Name 2",
                            /* isGATTConnected= */ true,
                            /* signalStrengthLevel= */ -1);
                    // Add non-connected device with signal strength level 1.
                    mChooserDialog.addOrUpdateDevice(
                            "id-3",
                            "Name 3",
                            /* isGATTConnected= */ false,
                            /* signalStrengthLevel= */ 1);
                    // Add connected device with signal strength level 1.
                    mChooserDialog.addOrUpdateDevice(
                            "id-4",
                            "Name 4",
                            /* isGATTConnected= */ true,
                            /* signalStrengthLevel= */ 1);
                });

        // After adding items to the dialog, the help message should be showing,
        // the progress spinner should disappear, the Commit button should still
        // be disabled (since nothing's selected), and the list view should
        // show.
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule.getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        DeviceItemAdapter itemAdapter =
                mChooserDialog.mItemChooserDialog.getItemAdapterForTesting();
        Assert.assertTrue(
                itemAdapter
                        .getItem(0)
                        .hasSameContents(
                                "id-1", "Name 1", /* icon= */ null, /* iconDescription= */ null));
        Assert.assertTrue(
                itemAdapter
                        .getItem(1)
                        .hasSameContents(
                                "id-2",
                                "Name 2",
                                mChooserDialog.mConnectedIcon,
                                mChooserDialog.mConnectedIconDescription));
        Assert.assertTrue(
                itemAdapter
                        .getItem(2)
                        .hasSameContents(
                                "id-3",
                                "Name 3",
                                mChooserDialog.mSignalStrengthLevelIcon[1],
                                sActivityTestRule
                                        .getActivity()
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals.signal_strength_level_n_bars, 1, 1)));
        // We show the connected icon even if the device has a signal strength.
        Assert.assertTrue(
                itemAdapter
                        .getItem(3)
                        .hasSameContents(
                                "id-4",
                                "Name 4",
                                mChooserDialog.mConnectedIcon,
                                mChooserDialog.mConnectedIconDescription));

        selectItem(2);

        Assert.assertEquals(BluetoothChooserEvent.SELECTED, mFinishedEventType);
        Assert.assertEquals("id-2", mFinishedDeviceId);
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
                () ->
                        mChooserDialog.notifyDiscoveryState(
                                BluetoothChooserDialog.DiscoveryMode.DISCOVERY_FAILED_TO_START));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            Assert.assertEquals(
                    removeLinkTags(
                            sActivityTestRule
                                    .getActivity()
                                    .getString(R.string.bluetooth_need_nearby_devices_permission)),
                    errorView.getText().toString());
        } else {
            Assert.assertEquals(
                    removeLinkTags(
                            sActivityTestRule
                                    .getActivity()
                                    .getString(R.string.bluetooth_need_location_permission)),
                    errorView.getText().toString());
        }

        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule
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

        Assert.assertEquals(1, mRestartSearchCount);
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule.getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());
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
                () ->
                        mChooserDialog.notifyDiscoveryState(
                                BluetoothChooserDialog.DiscoveryMode.DISCOVERY_FAILED_TO_START));

        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule
                                .getActivity()
                                .getString(R.string.bluetooth_need_location_services_on)),
                errorView.getText().toString());
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule
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
                                sActivityTestRule.getActivity(),
                                new Intent(LocationManager.MODE_CHANGED_ACTION)));

        Assert.assertEquals(1, mRestartSearchCount);
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule.getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());

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
        ThreadUtils.runOnUiThreadBlocking(() -> mChooserDialog.notifyAdapterTurnedOff());

        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule.getActivity().getString(R.string.bluetooth_adapter_off)),
                errorView.getText().toString());
        Assert.assertEquals(
                removeLinkTags(
                        sActivityTestRule
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
    @DisabledTest(message = "b/343347280")
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

        BluetoothChooserDialog dialog;
        dialog =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return BluetoothChooserDialog.create(
                                    mockWindowAndroid,
                                    "https://origin.example.com/",
                                    ConnectionSecurityLevel.SECURE,
                                    /* delegate= */ null,
                                    /* nativeUsbChooserDialogPtr= */ 42);
                        });
        Assert.assertNull(dialog);
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        Dialog mDialog;
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
