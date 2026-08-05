// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync_device_info.FormFactor;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for OtherDevicesShortcutController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.R)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_DYNAMIC_SHORTCUTS)
public class OtherDevicesShortcutControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String DEVICE_NAME_1 = "Device 1";
    private static final String DEVICE_NAME_2 = "Device 2";
    private static final String DEVICE_GUID_1 = "guid1";
    private static final String DEVICE_GUID_2 = "guid2";
    private static final String SHORTCUT_ID_1 =
            OtherDevicesShortcutController.SHORTCUT_ID_PREFIX + DEVICE_GUID_1;
    private static final String SHORTCUT_ID_2 =
            OtherDevicesShortcutController.SHORTCUT_ID_PREFIX + DEVICE_GUID_2;

    private static final String URL = "https://example.com";
    private static final String TITLE = "Title";

    @Mock private Profile mProfile;
    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private IdentityManager mIdentityManager;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();

        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    private Intent createShareTargetIntent(String shortcutId, String url, String title) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_SHORTCUT_ID, shortcutId);
        intent.putExtra(Intent.EXTRA_TEXT, url);
        intent.putExtra(Intent.EXTRA_SUBJECT, title);
        intent.setType("text/plain");
        return intent;
    }

    @Test
    public void testHandleShareTargetIntentForwarding() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Intent intent = createShareTargetIntent(SHORTCUT_ID_1, URL, TITLE);

        boolean handled =
                OtherDevicesShortcutController.handleShareTargetIntentForwarding(activity, intent);

        org.junit.Assert.assertTrue(handled);

        Intent startedIntent =
                org.robolectric.Shadows.shadowOf(
                                org.robolectric.RuntimeEnvironment.getApplication())
                        .getNextStartedActivity();
        assertNotNull(startedIntent);
        // The forwarded intent should have its target class set to SendTabToSelfShareTargetActivity
        // but otherwise match the original intent.
        assertEquals(
                SendTabToSelfShareTargetActivity.class.getName(),
                startedIntent.getComponent().getClassName());
        assertEquals(Intent.ACTION_SEND, startedIntent.getAction());
        assertEquals("text/plain", startedIntent.getType());
        assertEquals(SHORTCUT_ID_1, startedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_ID));
        assertEquals(URL, startedIntent.getStringExtra(Intent.EXTRA_TEXT));
        assertEquals(TITLE, startedIntent.getStringExtra(Intent.EXTRA_SUBJECT));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_DYNAMIC_SHORTCUTS)
    public void testHandleShareTargetIntentForwarding_FeatureDisabled() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Intent intent = createShareTargetIntent(SHORTCUT_ID_1, URL, TITLE);

        boolean handled =
                OtherDevicesShortcutController.handleShareTargetIntentForwarding(activity, intent);

        org.junit.Assert.assertFalse(handled);
    }

    @Test
    public void handleShareTargetIntent() {
        List<TargetDeviceInfo> devices = new ArrayList<>();
        devices.add(new TargetDeviceInfo("Device 1", DEVICE_GUID_1, FormFactor.PHONE, "Just now"));
        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);

        // Instantiate controller to populate shortcuts in ShortcutManager.
        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Intent intent = createShareTargetIntent(SHORTCUT_ID_1, URL, TITLE);

        OtherDevicesShortcutController.handleShareTargetIntent(activity, intent, mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(null),
                        eq(DEVICE_GUID_1),
                        eq(URL),
                        eq(TITLE),
                        any(),
                        eq(ShareEntryPoint.SHARE_SHEET_DIRECT_SHARE));
    }

    @Test
    public void handleShareTargetIntent_RejectUnknownDevice() {
        List<TargetDeviceInfo> devices = new ArrayList<>();
        // No devices synced.
        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);

        // Instantiate controller to populate shortcuts in ShortcutManager (will be empty).
        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Intent intent = createShareTargetIntent(SHORTCUT_ID_1, URL, TITLE);

        OtherDevicesShortcutController.handleShareTargetIntent(activity, intent, mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_DYNAMIC_SHORTCUTS)
    public void handleShareTargetIntent_FeatureDisabled() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Intent intent = createShareTargetIntent(SHORTCUT_ID_1, URL, TITLE);

        OtherDevicesShortcutController.handleShareTargetIntent(activity, intent, mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativeMock, org.mockito.Mockito.never())
                .sendTabToDevice(any(), any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    public void testHandleLauncherShortcutIntent_RelaunchesAsTrusted() {
        List<TargetDeviceInfo> devices = new ArrayList<>();
        devices.add(
                new TargetDeviceInfo(DEVICE_NAME_1, DEVICE_GUID_1, FormFactor.PHONE, "Just now"));
        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Intent intent = new Intent(OtherDevicesShortcutController.ACTION_OPEN_RECENT_TABS);
        intent.putExtra(OtherDevicesShortcutController.EXTRA_DEVICE_GUID, DEVICE_GUID_1);

        boolean handled =
                OtherDevicesShortcutController.handleLauncherShortcutIntent(activity, intent);

        assertTrue(handled);

        Intent startedIntent =
                org.robolectric.Shadows.shadowOf(
                                org.robolectric.RuntimeEnvironment.getApplication())
                        .getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        assertEquals(
                "chrome-native://recent-tabs/#" + DEVICE_GUID_1, startedIntent.getDataString());
        // Verify it was made trusted.
        assertTrue(org.chromium.chrome.browser.IntentHandler.wasIntentSenderChrome(startedIntent));
    }

    private ShortcutInfo findShortcut(String id) {
        ShortcutManager shortcutManager = mContext.getSystemService(ShortcutManager.class);
        return shortcutManager.getDynamicShortcuts().stream()
                .filter(s -> s.getId().equals(id))
                .findFirst()
                .orElse(null);
    }

    private boolean isValidOtherDeviceShortcut(ShortcutInfo shortcut) {
        if (shortcut == null) return false;

        if (!shortcut.isLongLived()) return false;
        if (!shortcut.getCategories().contains(OtherDevicesShortcutController.CATEGORY)) {
            return false;
        }

        Intent intent = shortcut.getIntent();
        if (intent == null) return false;

        if (!intent.getAction().equals(OtherDevicesShortcutController.ACTION_OPEN_RECENT_TABS)) {
            return false;
        }

        if (!intent.getComponent()
                .getClassName()
                .equals("org.chromium.chrome.browser.LauncherShortcutActivity")) {
            return false;
        }

        return true;
    }

    private String getGuidFromShortcut(ShortcutInfo shortcut) {
        return shortcut.getIntent()
                .getStringExtra(OtherDevicesShortcutController.EXTRA_DEVICE_GUID);
    }

    @Test
    public void testUpdateShortcuts() {
        List<TargetDeviceInfo> devices = new ArrayList<>();
        devices.add(
                new TargetDeviceInfo(DEVICE_NAME_1, DEVICE_GUID_1, FormFactor.PHONE, "Just now"));
        devices.add(
                new TargetDeviceInfo(DEVICE_NAME_2, DEVICE_GUID_2, FormFactor.DESKTOP, "Just now"));

        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);
        when(mNativeMock.addDeviceInfoObserver(any(), any())).thenReturn(123L);
        when(mNativeMock.addModelObserver(any(), any())).thenReturn(456L);

        // This will trigger the first updateShortcuts() in constructor.
        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        ShortcutManager shortcutManager = mContext.getSystemService(ShortcutManager.class);
        assertEquals(2, shortcutManager.getDynamicShortcuts().size());

        ShortcutInfo shortcut1 = findShortcut(SHORTCUT_ID_1);
        assertTrue(isValidOtherDeviceShortcut(shortcut1));
        assertEquals(DEVICE_NAME_1, shortcut1.getShortLabel());
        assertEquals(DEVICE_NAME_1, shortcut1.getLongLabel());
        assertEquals(DEVICE_GUID_1, getGuidFromShortcut(shortcut1));

        ShortcutInfo shortcut2 = findShortcut(SHORTCUT_ID_2);
        assertTrue(isValidOtherDeviceShortcut(shortcut2));
        assertEquals(DEVICE_NAME_2, shortcut2.getShortLabel());
        assertEquals(DEVICE_NAME_2, shortcut2.getLongLabel());
        assertEquals(DEVICE_GUID_2, getGuidFromShortcut(shortcut2));

        controller.destroy();
        verify(mNativeMock).removeDeviceInfoObserver(eq(123L));
        verify(mNativeMock).removeModelObserver(eq(456L));
    }

    @Test
    public void testUpdateShortcuts_WithGivenName() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

        List<TargetDeviceInfo> devices = new ArrayList<>();
        devices.add(
                new TargetDeviceInfo(DEVICE_NAME_1, DEVICE_GUID_1, FormFactor.PHONE, "Just now"));

        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);

        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        ShortcutInfo shortcut1 = findShortcut(SHORTCUT_ID_1);
        assertTrue(isValidOtherDeviceShortcut(shortcut1));
        assertEquals(DEVICE_NAME_1, shortcut1.getShortLabel());
        assertEquals(
                TestAccounts.ACCOUNT1.getGivenName() + " • " + DEVICE_NAME_1,
                shortcut1.getLongLabel());
    }

    @Test
    public void testUpdateShortcuts_WithoutGivenName() {
        when(mIdentityManager.getPrimaryAccountInfo())
                .thenReturn(TestAccounts.TEST_ACCOUNT_NO_NAME);

        List<TargetDeviceInfo> devices = new ArrayList<>();
        devices.add(
                new TargetDeviceInfo(DEVICE_NAME_1, DEVICE_GUID_1, FormFactor.PHONE, "Just now"));

        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);

        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        ShortcutInfo shortcut1 = findShortcut(SHORTCUT_ID_1);
        assertTrue(isValidOtherDeviceShortcut(shortcut1));
        assertEquals(DEVICE_NAME_1, shortcut1.getShortLabel());
        // Since the test user doesn't have a given name, the long label should fall back to just
        // the device name.
        assertEquals(DEVICE_NAME_1, shortcut1.getLongLabel());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_DYNAMIC_SHORTCUTS)
    public void testFeatureDisabled_NoShortcutsRegistered() {
        List<TargetDeviceInfo> devices = new ArrayList<>();
        devices.add(
                new TargetDeviceInfo(DEVICE_NAME_1, DEVICE_GUID_1, FormFactor.PHONE, "Just now"));

        when(mNativeMock.getAllTargetDeviceInfos(mProfile)).thenReturn(devices);

        OtherDevicesShortcutController controller = new OtherDevicesShortcutController(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        ShortcutManager shortcutManager = mContext.getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        assertEquals(0, shortcuts.size());

        // Verify observers were NOT registered.
        verify(mNativeMock, never()).addDeviceInfoObserver(any(), any());
        verify(mNativeMock, never()).addModelObserver(any(), any());
    }
}
