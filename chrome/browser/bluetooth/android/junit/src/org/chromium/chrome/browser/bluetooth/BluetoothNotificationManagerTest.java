// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.bluetooth;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Tests BluetoothNotificationManager behaviour and its delegate. */
@RunWith(BaseRobolectricTestRunner.class)
public class BluetoothNotificationManagerTest {
    private static final int NOTIFICATION_ID = 0;
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final String TEST_URL_FORMATTED =
            UrlFormatter.formatUrlForSecurityDisplay(TEST_URL, SchemeDisplay.OMIT_HTTP_AND_HTTPS);

    private MockNotificationManagerProxy mMockNotificationManager;
    private BluetoothNotificationManagerDelegate mDelegate =
            new BluetoothNotificationManagerDelegate() {
                @Override
                public Intent createTrustedBringTabToFrontIntent(int tabId) {
                    mTabsBroughtToFront++;
                    return new Intent();
                }

                @Override
                public void stopSelf() {
                    mServiceStopped = true;
                }

                @Override
                public void stopSelf(int startId) {
                    mServiceStopped = true;
                    mLastStartId = startId;
                }
            };

    private static class FakeService {}

    private BluetoothNotificationManager mManager;
    private boolean mServiceStopped;
    private int mLastStartId;
    private int mTabsBroughtToFront;

    private Intent createIntent(@BluetoothNotificationManager.BluetoothType int bluetoothType) {
        return createIntent(bluetoothType, /* isIncognito= */ false);
    }

    private Intent createIntent(
            @BluetoothNotificationManager.BluetoothType int bluetoothType, boolean isIncognito) {
        Intent intent = new Intent(mock(Context.class), FakeService.class);
        intent.setAction(BluetoothNotificationManager.ACTION_BLUETOOTH_UPDATE);
        intent.putExtra(BluetoothNotificationManager.NOTIFICATION_ID_EXTRA, NOTIFICATION_ID);
        intent.putExtra(BluetoothNotificationManager.NOTIFICATION_URL_EXTRA, TEST_URL.getSpec());
        intent.putExtra(
                BluetoothNotificationManager.NOTIFICATION_BLUETOOTH_TYPE_EXTRA, bluetoothType);
        intent.putExtra(BluetoothNotificationManager.NOTIFICATION_IS_INCOGNITO, isIncognito);
        return intent;
    }

    private void assertNotificationEquals(String expectedTitle, String expectedText) {
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                expectedTitle,
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                expectedText,
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mMockNotificationManager = new MockNotificationManagerProxy();
        mManager = new BluetoothNotificationManager(mMockNotificationManager, mDelegate);
    }

    @Test
    public void test_nullIntentStopsService() {
        mServiceStopped = false;
        mManager.onStartCommand(null, 0, /* startId= */ 0);
        assertTrue(mServiceStopped);
        assertEquals(0, mLastStartId);
        assertEquals(0, mTabsBroughtToFront);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void test_nullIntentWithStartIdStopsService() {
        mServiceStopped = false;
        mManager.onStartCommand(null, 0, /* startId= */ 42);
        assertTrue(mServiceStopped);
        assertEquals(0, mLastStartId);
        assertEquals(0, mTabsBroughtToFront);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void test_connectedShowsNotification() {
        Intent intent = createIntent(BluetoothNotificationManager.BluetoothType.IS_CONNECTED);
        mManager.onStartCommand(intent, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals(
                "Connected to a Bluetooth device", "Tap to return to " + TEST_URL_FORMATTED);
    }

    @Test
    public void test_scanningShowsNotification() {
        Intent intent = createIntent(BluetoothNotificationManager.BluetoothType.IS_SCANNING);
        mManager.onStartCommand(intent, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals(
                "Scanning for Bluetooth devices", "Tap to return to " + TEST_URL_FORMATTED);
    }

    @Test
    public void test_connectedAndScanningShowsOneNotification() {
        Intent intent1 = createIntent(BluetoothNotificationManager.BluetoothType.IS_CONNECTED);
        mManager.onStartCommand(intent1, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals(
                "Connected to a Bluetooth device", "Tap to return to " + TEST_URL_FORMATTED);

        Intent intent2 = createIntent(BluetoothNotificationManager.BluetoothType.IS_SCANNING);
        mManager.onStartCommand(intent2, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(2, mTabsBroughtToFront);
        assertNotificationEquals(
                "Scanning for Bluetooth devices", "Tap to return to " + TEST_URL_FORMATTED);
    }

    @Test
    public void test_disconnectHidesNotification() {
        Intent intent1 = createIntent(BluetoothNotificationManager.BluetoothType.IS_CONNECTED);
        mManager.onStartCommand(intent1, 0, /* startId= */ 42);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertEquals(0, mLastStartId);
        assertNotificationEquals(
                "Connected to a Bluetooth device", "Tap to return to " + TEST_URL_FORMATTED);

        Intent intent2 = createIntent(BluetoothNotificationManager.BluetoothType.NO_BLUETOOTH);
        mManager.onStartCommand(intent2, 0, /* startId= */ 42);

        assertTrue(mServiceStopped);
        assertEquals(42, mLastStartId);
        assertEquals(1, mTabsBroughtToFront);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void test_connectedInIncognitoShowsNotification() {
        Intent intent =
                createIntent(
                        BluetoothNotificationManager.BluetoothType.IS_CONNECTED,
                        /* isIncognito= */ true);
        mManager.onStartCommand(intent, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals("Connected to a Bluetooth device", "Tap to return to the site");
    }

    @Test
    public void test_scanningInIncognitoShowsNotification() {
        Intent intent =
                createIntent(
                        BluetoothNotificationManager.BluetoothType.IS_SCANNING,
                        /* isIncognito= */ true);
        mManager.onStartCommand(intent, 0, 0);

        assertFalse(mServiceStopped);
        assertEquals(1, mTabsBroughtToFront);
        assertNotificationEquals("Scanning for Bluetooth devices", "Tap to return to the site");
    }
}
