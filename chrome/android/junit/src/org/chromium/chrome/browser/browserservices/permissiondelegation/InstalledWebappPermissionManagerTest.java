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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;

import java.util.Arrays;
import java.util.Collection;

/** Tests for {@link InstalledWebappPermissionManager}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstalledWebappPermissionManagerTest {

    @ParameterizedRobolectricTestRunner.Parameters
    public static Collection testCases() {
        return Arrays.asList(
                new Object[][] {
                    {ContentSettingsType.GEOLOCATION},
                    {ContentSettingsType.GEOLOCATION_WITH_OPTIONS},
                });
    }

    private final Origin mOrigin = Origin.create("https://www.website.com");
    private static final String PACKAGE_NAME = "com.package.name";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Mock public InstalledWebappPermissionStore mStore;

    private ShadowPackageManager mShadowPackageManager;

    private final @ContentSettingsType.EnumType int mType;

    public InstalledWebappPermissionManagerTest(@ContentSettingsType.EnumType int type) {
        mType = type;
    }

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        FeatureOverrides.newBuilder()
                .flag(
                        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION,
                        mType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS)
                .apply();

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);

        when(mStore.getDelegatePackageName(eq(mOrigin))).thenReturn(PACKAGE_NAME);
        WebappRegistry.getInstance().setPermissionStoreForTesting(mStore);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationDelegationNotEnabled_whenClientNotRequested() {
        setNoPermissionRequested();

        assertEquals(
                ContentSetting.DEFAULT,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));
        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAllowed_whenClientAllowed() {
        setClientLocationPermission(true);
        setStoredLocationPermission(ContentSetting.BLOCK);

        assertEquals(
                ContentSetting.ALLOW,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));
        verifyLocationPermissionUpdated(ContentSetting.ALLOW);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionBlocked_whenClientBlocked() {
        setClientLocationPermission(false);
        setStoredLocationPermission(ContentSetting.ALLOW);

        assertEquals(
                ContentSetting.BLOCK,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));
        verifyLocationPermissionUpdated(ContentSetting.BLOCK);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAsk_whenNoPreviousPermission() {
        setClientLocationPermission(false);
        setStoredLocationPermission(null);

        assertEquals(
                ContentSetting.ASK, InstalledWebappPermissionManager.getPermission(mType, mOrigin));
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
                ContentSetting.ASK, InstalledWebappPermissionManager.getPermission(mType, mOrigin));
        verifyPermissionNotUpdated();

        setStoredLocationPermission(ContentSetting.ALLOW);
        setClientLocationPermission(false);
        assertEquals(
                ContentSetting.ASK, InstalledWebappPermissionManager.getPermission(mType, mOrigin));
        verifyPermissionNotUpdated();

        setStoredLocationPermission(ContentSetting.BLOCK);
        setClientLocationPermission(false);
        assertEquals(
                ContentSetting.ASK, InstalledWebappPermissionManager.getPermission(mType, mOrigin));
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
                ContentSetting.ALLOW,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));

        packageInfo.requestedPermissions = new String[] {ACCESS_FINE_LOCATION};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSetting.ALLOW,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));

        // When one of the two location permission is granted, return ALLOW.
        packageInfo.requestedPermissions =
                new String[] {ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION};
        packageInfo.requestedPermissionsFlags =
                new int[] {0, PackageInfo.REQUESTED_PERMISSION_GRANTED};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSetting.ALLOW,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));

        packageInfo.requestedPermissions = new String[] {};
        mShadowPackageManager.installPackage(packageInfo);
        assertEquals(
                ContentSetting.DEFAULT,
                InstalledWebappPermissionManager.getPermission(mType, mOrigin));
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

    private void setStoredLocationPermission(@ContentSetting Integer settingValue) {
        when(mStore.getPermission(eq(mType), eq(mOrigin))).thenReturn(settingValue);
    }

    private void verifyPermissionNotUpdated() {
        verify(mStore, never())
                .setStateForOrigin(any(), anyString(), anyString(), anyInt(), anyInt());
    }

    private void verifyLocationPermissionUpdated(@ContentSetting int settingValue) {
        verify(mStore)
                .setStateForOrigin(
                        eq(mOrigin), eq(PACKAGE_NAME), anyString(), eq(mType), eq(settingValue));
    }
}
