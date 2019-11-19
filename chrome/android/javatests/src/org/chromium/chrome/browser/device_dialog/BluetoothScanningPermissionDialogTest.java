// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Dialog;
import android.support.test.filters.LargeTest;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.bluetooth_scanning.Event;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Tests for the BluetoothScanningPermissionDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BluetoothScanningPermissionDialogTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public JniMocker mocker = new JniMocker();

    private int mFinishedEventType = -1;

    private ActivityWindowAndroid mWindowAndroid;
    private BluetoothScanningPermissionDialog mPermissionDialog;

    private class TestBluetoothScanningPermissionDialogJni
            implements BluetoothScanningPermissionDialog.Natives {
        @Override
        public void onDialogFinished(long nativeBluetoothScanningPromptAndroid, int eventType) {
            mFinishedEventType = eventType;
        }
    }

    @Before
    public void setUp() throws Exception {
        mocker.mock(BluetoothScanningPermissionDialogJni.TEST_HOOKS,
                new TestBluetoothScanningPermissionDialogJni());
        mActivityTestRule.startMainActivityOnBlankPage();
        mPermissionDialog = createDialog();
    }

    private BluetoothScanningPermissionDialog createDialog() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mWindowAndroid = new ActivityWindowAndroid(mActivityTestRule.getActivity());
            BluetoothScanningPermissionDialog dialog = new BluetoothScanningPermissionDialog(
                    mWindowAndroid, "https://origin.example.com/", ConnectionSecurityLevel.SECURE,
                    /*nativeBluetoothScanningPermissionDialogPtr=*/42);
            return dialog;
        });
    }

    @Test
    @LargeTest
    public void testAddDevice() {
        Dialog dialog = mPermissionDialog.getDialogForTesting();

        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button allowButton = (Button) dialog.findViewById(R.id.allow);
        final Button blockButton = (Button) dialog.findViewById(R.id.block);

        // The 'Allow' and 'Block' button should be visible and enabled.
        Assert.assertEquals(View.VISIBLE, allowButton.getVisibility());
        Assert.assertEquals(View.VISIBLE, blockButton.getVisibility());
        Assert.assertTrue(allowButton.isEnabled());
        Assert.assertTrue(blockButton.isEnabled());
        // The list view should be hidden since there is no item in the list.
        Assert.assertEquals(View.GONE, items.getVisibility());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPermissionDialog.addOrUpdateDevice("device_id_0", "device_name_0");
            mPermissionDialog.addOrUpdateDevice("device_id_1", "device_name_1");
            mPermissionDialog.addOrUpdateDevice("device_id_2", "device_name_2");
        });

        // The 'Allow' and 'Block' button should still be visible and enabled.
        Assert.assertEquals(View.VISIBLE, allowButton.getVisibility());
        Assert.assertEquals(View.VISIBLE, blockButton.getVisibility());
        Assert.assertTrue(allowButton.isEnabled());
        Assert.assertTrue(blockButton.isEnabled());
        // After adding items to the dialog, the list view should show the list of devices.
        Assert.assertEquals(View.VISIBLE, items.getVisibility());

        DeviceItemAdapter itemAdapter = mPermissionDialog.getItemAdapterForTesting();
        Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                "device_id_0", "device_name_0", /*icon=*/null, /*iconDescription=*/null));
        Assert.assertTrue(itemAdapter.getItem(1).hasSameContents(
                "device_id_1", "device_name_1", /*icon=*/null, /*iconDescription=*/null));
        Assert.assertTrue(itemAdapter.getItem(2).hasSameContents(
                "device_id_2", "device_name_2", /*icon=*/null, /*iconDescription=*/null));
    }

    @Test
    @LargeTest
    public void testCancelPermissionDialogWithoutClickingAnyButton() {
        Dialog dialog = mPermissionDialog.getDialogForTesting();

        dialog.cancel();

        CriteriaHelper.pollUiThread(Criteria.equals(Event.CANCELED, () -> mFinishedEventType));
    }
}
