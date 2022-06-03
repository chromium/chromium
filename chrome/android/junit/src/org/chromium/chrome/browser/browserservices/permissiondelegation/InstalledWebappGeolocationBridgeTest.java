// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Bundle;

import androidx.browser.trusted.TrustedWebActivityCallback;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Tests for {@link InstalledWebappGeolocationBridge}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_LOCATION_DELEGATION)
public class InstalledWebappGeolocationBridgeTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final Origin OTHER_ORIGIN = Origin.create("https://www.other.website.com");
    private static final String EXTRA_CALLBACK = "extraCallback";

    private static final long NATIVE_POINTER = 12;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private TrustedWebActivityClient mTrustedWebActivityClient;
    @Mock
    private InstalledWebappGeolocationBridge.Natives mNativeMock;

    private InstalledWebappGeolocationBridge mGeolocation;

    private boolean mIsHighAccuracy;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(InstalledWebappGeolocationBridgeJni.TEST_HOOKS, mNativeMock);

        mGeolocation = new InstalledWebappGeolocationBridge(
                NATIVE_POINTER, ORIGIN, mTrustedWebActivityClient);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getLocationError_whenClientDoesntHaveService() {
        uninstallTrustedWebActivityService(ORIGIN);
        mGeolocation.start(false /* HighAccuracy */);
        verifyGetLocationError();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getLocationUpdate_afterStartListening() {
        installTrustedWebActivityService(ORIGIN);
        mGeolocation.start(false /* HighAccuracy */);
        verifyGetLocationUpdate();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noLocationUpdate_stopBeforeStart() {
        installTrustedWebActivityService(ORIGIN);
        mGeolocation.stopAndDestroy();
        mGeolocation.start(false /* HighAccuracy */);
        verifyNoLocationUpdate();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getLocationError_whenOnlytherClientHasService() {
        installTrustedWebActivityService(OTHER_ORIGIN);
        uninstallTrustedWebActivityService(ORIGIN);
        mGeolocation.start(false /* HighAccuracy */);
        verifyGetLocationError();
        verifyNoLocationUpdate();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void changeHighAccuracyAfterStart() {
        installTrustedWebActivityService(ORIGIN);
        mGeolocation.start(false /* HighAccuracy */);
        assertFalse(mIsHighAccuracy);
        mGeolocation.start(true /* HighAccuracy */);
        assertTrue(mIsHighAccuracy);
    }

    /** "Installs" a Trusted Web Activity Service for the origin. */
    @SuppressWarnings("unchecked")
    private void installTrustedWebActivityService(Origin origin) {
        doAnswer(invocation -> {
            TrustedWebActivityCallback callback = invocation.getArgument(2);
            mIsHighAccuracy = invocation.getArgument(1);

            Bundle result = new Bundle();
            // Put arbitrary value to test the result bundle is converted correctly.
            // These value may not be valid geolocation data.
            result.putDouble("latitude", 1.0d);
            result.putDouble("longitude", -2.1d);
            result.putLong("timeStamp", 30);
            result.putDouble("altitude", 4.0d);
            result.putDouble("accuracy", 5.3d);
            result.putDouble("bearing", -6.4d);
            result.putDouble("speed", 7.5d);

            callback.onExtraCallback(
                    InstalledWebappGeolocationBridge.EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK, result);
            return true;
        })
                .when(mTrustedWebActivityClient)
                .startListeningLocationUpdates(eq(origin), anyBoolean(), any());
    }

    private void uninstallTrustedWebActivityService(Origin origin) {
        doAnswer(invocation -> {
            TrustedWebActivityCallback callback = invocation.getArgument(2);
            Bundle error = new Bundle();
            error.putString("message", "any errro message");
            callback.onExtraCallback(
                    InstalledWebappGeolocationBridge.EXTRA_NEW_LOCATION_ERROR_CALLBACK, error);
            return true;
        })
                .when(mTrustedWebActivityClient)
                .startListeningLocationUpdates(eq(origin), anyBoolean(), any());
    }

    // Verify native gets location update with correct value.
    private void verifyGetLocationUpdate() {
        verify(mNativeMock)
                .onNewLocationAvailable(eq(NATIVE_POINTER), eq(1.0d), eq(-2.1d), eq(0.03d),
                        eq(true), eq(4.0d), eq(true), eq(5.3d), eq(true), eq(-6.4d), eq(true),
                        eq(7.5d));
    }

    private void verifyGetLocationError() {
        verify(mNativeMock).onNewErrorAvailable(eq(NATIVE_POINTER), anyString());
    }

    private void verifyNoLocationUpdate() {
        verify(mNativeMock, never())
                .onNewLocationAvailable(anyInt(), anyDouble(), anyDouble(), anyDouble(),
                        anyBoolean(), anyDouble(), anyBoolean(), anyDouble(), anyBoolean(),
                        anyDouble(), anyBoolean(), anyDouble());
    }
}
