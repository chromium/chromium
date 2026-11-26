// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.synced_set_up.PrefToValueMapBridge;
import org.chromium.components.sync_preferences.synced_set_up.PrefToValueMapBridgeTestSupport;

import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link SyncedSetUpUtilsBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SyncedSetUpUtilsBridgeUnitTest {
    private static final long PROFILE_PTR = 10L;
    private static final long TRACKER_PTR = 20L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SyncedSetUpUtilsBridge.Natives mNativesMock;
    @Mock private CrossDevicePrefTracker mCrossDevicePrefTracker;
    @Mock private Profile mProfile;

    private final AtomicReference<PrefToValueMapBridge> mCapturedBridge = new AtomicReference<>();

    @Before
    public void setUp() {
        SyncedSetUpUtilsBridgeJni.setInstanceForTesting(mNativesMock);
        PrefToValueMapBridgeTestSupport.stubInitForTesting(mCapturedBridge::set);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(TRACKER_PTR);
        when(mProfile.getNativeBrowserContextPointer()).thenReturn(PROFILE_PTR);
    }

    @Test
    public void testGetCrossDevicePrefsFromRemoteDevice_ZeroTrackerPtr() {
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        Map<String, Object> prefs =
                SyncedSetUpUtilsBridge.getCrossDevicePrefsFromRemoteDevice(
                        mCrossDevicePrefTracker, mProfile);

        assertTrue(prefs.isEmpty());
        assertNull(mCapturedBridge.get());
        verify(mProfile, never()).getNativeBrowserContextPointer();
        verifyNoInteractions(mNativesMock);
    }

    @Test
    public void testGetCrossDevicePrefsFromRemoteDevice_PopulatesAndDestroysBridge() {
        doAnswer(
                        inv -> {
                            PrefToValueMapBridge bridge = mCapturedBridge.get();
                            assertNotNull(bridge);
                            assertSame(bridge.getNativeBridgePtr(), inv.getArgument(2));
                            PrefToValueMapBridgeTestSupport.putStringForTesting(
                                    bridge, "theme", "dark");
                            PrefToValueMapBridgeTestSupport.putIntForTesting(bridge, "count", 3);
                            PrefToValueMapBridgeTestSupport.putBooleanForTesting(
                                    bridge, "enabled", true);
                            return null;
                        })
                .when(mNativesMock)
                .getCrossDevicePrefsFromRemoteDevice(eq(PROFILE_PTR), eq(TRACKER_PTR), any());

        Map<String, Object> prefs =
                SyncedSetUpUtilsBridge.getCrossDevicePrefsFromRemoteDevice(
                        mCrossDevicePrefTracker, mProfile);

        assertEquals(Map.of("theme", "dark", "count", 3, "enabled", true), prefs);
        PrefToValueMapBridge bridge = mCapturedBridge.get();
        assertNotNull(bridge);
        assertThrows(IllegalStateException.class, bridge::getNativeBridgePtr);
    }

    @Test
    public void testGetCrossDevicePrefsFromRemoteDevice_DestroysBridgeWhenJniThrows() {
        doThrow(new RuntimeException("jni failure"))
                .when(mNativesMock)
                .getCrossDevicePrefsFromRemoteDevice(eq(PROFILE_PTR), eq(TRACKER_PTR), any());

        assertThrows(
                RuntimeException.class,
                () ->
                        SyncedSetUpUtilsBridge.getCrossDevicePrefsFromRemoteDevice(
                                mCrossDevicePrefTracker, mProfile));

        PrefToValueMapBridge bridge = mCapturedBridge.get();
        assertNotNull(bridge);
        assertThrows(IllegalStateException.class, bridge::getNativeBridgePtr);
    }

    @Test
    public void testGetCrossDevicePrefsFromRemoteDevice_ProfileThrowsBeforeBridgeInit() {
        when(mProfile.getNativeBrowserContextPointer())
                .thenThrow(new IllegalStateException("destroyed profile"));

        assertThrows(
                IllegalStateException.class,
                () ->
                        SyncedSetUpUtilsBridge.getCrossDevicePrefsFromRemoteDevice(
                                mCrossDevicePrefTracker, mProfile));

        assertNull(mCapturedBridge.get());
        verifyNoInteractions(mNativesMock);
    }
}
