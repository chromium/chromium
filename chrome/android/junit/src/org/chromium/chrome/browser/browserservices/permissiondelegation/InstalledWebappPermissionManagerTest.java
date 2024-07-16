// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;

import dagger.Lazy;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

/** Tests for {@link InstalledWebappPermissionManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstalledWebappPermissionManagerTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String PACKAGE_NAME = "com.package.name";

    @Mock public InstalledWebappPermissionStore mStore;
    @Mock public Lazy<NotificationChannelPreserver> mPreserver;

    private InstalledWebappPermissionManager mPermissionManager;

    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);

        Context context = mock(Context.class);

        when(mStore.getDelegatePackageName(eq(ORIGIN))).thenReturn(PACKAGE_NAME);

        mPermissionManager = new InstalledWebappPermissionManager(context, mStore, mPreserver);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationDelegationNotEnabled_whenClientNotRequested() {
        setNoPermissionRequested();

        assertEquals(
                ContentSettingValues.DEFAULT,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAllowed_whenClientAllowed() {
        setClientLocationPermission(true);
        setStoredLocationPermission(ContentSettingValues.BLOCK);

        assertEquals(
                ContentSettingValues.ALLOW,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyLocationPermissionUpdated(ContentSettingValues.ALLOW);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionBlocked_whenClientBlocked() {
        setClientLocationPermission(false);
        setStoredLocationPermission(ContentSettingValues.ALLOW);

        assertEquals(
                ContentSettingValues.BLOCK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyLocationPermissionUpdated(ContentSettingValues.BLOCK);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAsk_whenNoPreviousPermission() {
        setClientLocationPermission(false);
        setStoredLocationPermission(null);

        assertEquals(
                ContentSettingValues.ASK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAsk_whenNoPermissionOnAndroidR() {
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", 30 /*Build.VERSION_CODES.R*/);

        setStoredLocationPermission(null);
        setClientLocationPermission(false);
        assertEquals(
                ContentSettingValues.ASK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();

        setStoredLocationPermission(ContentSettingValues.ALLOW);
        setClientLocationPermission(false);
        assertEquals(
                ContentSettingValues.ASK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();

        setStoredLocationPermission(ContentSettingValues.BLOCK);
        setClientLocationPermission(false);
        assertEquals(
                ContentSettingValues.ASK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationDelegationEnabled_withCoarseOrFineLocation() {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = PACKAGE_NAME;

        packageInfo.requestedPermissions = new String[] {ACCESS_COARSE_LOCATION};
        packageInfo.requestedPermissionsFlags =
                new int[] {PackageInfo.REQUESTED_PERMISSION_GRANTED};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSettingValues.ALLOW,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));

        packageInfo.requestedPermissions = new String[] {ACCESS_FINE_LOCATION};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSettingValues.ALLOW,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));

        // When one of the two location permission is granted, return ALLOW.
        packageInfo.requestedPermissions =
                new String[] {ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION};
        packageInfo.requestedPermissionsFlags =
                new int[] {0, PackageInfo.REQUESTED_PERMISSION_GRANTED};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSettingValues.ALLOW,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));

        packageInfo.requestedPermissions = new String[] {};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSettingValues.DEFAULT,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
    }

    private void setNoPermissionRequested() {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = PACKAGE_NAME;
        packageInfo.requestedPermissions = new String[0];
        packageInfo.requestedPermissionsFlags = new int[0];
        mShadowPackageManager.installPackage(packageInfo);
    }

    private void setClientLocationPermission(boolean enabled) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = PACKAGE_NAME;
        packageInfo.requestedPermissions = new String[] {ACCESS_COARSE_LOCATION};
        packageInfo.requestedPermissionsFlags =
                new int[] {(enabled ? PackageInfo.REQUESTED_PERMISSION_GRANTED : 0)};
        mShadowPackageManager.installPackage(packageInfo);
    }

    private void setStoredLocationPermission(@ContentSettingValues Integer settingValue) {
        when(mStore.getPermission(eq(ContentSettingsType.GEOLOCATION), eq(ORIGIN)))
                .thenReturn(settingValue);
    }

    private void verifyPermissionNotUpdated() {
        verify(mStore, never())
                .setStateForOrigin(any(), anyString(), anyString(), anyInt(), anyInt());
    }

    private void verifyLocationPermissionUpdated(@ContentSettingValues int settingValue) {
        verify(mStore)
                .setStateForOrigin(
                        eq(ORIGIN),
                        eq(PACKAGE_NAME),
                        anyString(),
                        eq(ContentSettingsType.GEOLOCATION),
                        eq(settingValue));
    }
}
