// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric unit tests for {@link DeviceInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DeviceInfoUnitTest {
    private static final String GMS_PACKAGE_NAME = "com.google.android.gms";

    @Before
    public void setUp() {
        DeviceInfo.resetGmsInfoForTesting();
    }

    @After
    public void tearDown() {
        DeviceInfo.resetGmsInfoForTesting();
    }

    @Test
    public void testDeviceInfoInitializationDoesNotInitializeGmsInfo() {
        DeviceInfo.isFoldable();

        assertFalse(DeviceInfo.isGmsInfoInitializedForTesting());
    }

    @Test
    public void testGmsInfoIsInitializedLazilyAndCached() {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = GMS_PACKAGE_NAME;
        packageInfo.versionCode = 12345;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = GMS_PACKAGE_NAME;
        ShadowPackageManager packageManager =
                shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        packageManager.installPackage(packageInfo);

        assertFalse(DeviceInfo.isGmsInfoInitializedForTesting());
        assertEquals("12345", DeviceInfo.getGmsVersionCode());
        ApplicationInfo applicationInfo = DeviceInfo.getGmsAppInfo();
        assertNotNull(applicationInfo);
        assertEquals(GMS_PACKAGE_NAME, applicationInfo.packageName);
        assertTrue(DeviceInfo.isGmsInfoInitializedForTesting());

        packageManager.removePackage(GMS_PACKAGE_NAME);
        assertEquals("12345", DeviceInfo.getGmsVersionCode());
        assertEquals(applicationInfo, DeviceInfo.getGmsAppInfo());
    }

    @Test
    public void testGetAidlInfoInitializesGmsInfo() {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = GMS_PACKAGE_NAME;
        packageInfo.versionCode = 12345;
        ShadowPackageManager packageManager =
                shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        packageManager.installPackage(packageInfo);

        assertFalse(DeviceInfo.isGmsInfoInitializedForTesting());

        assertEquals("12345", DeviceInfo.getAidlInfo().gmsVersionCode);
        assertTrue(DeviceInfo.isGmsInfoInitializedForTesting());
    }

    @Test
    public void testGmsVersionOverrideDoesNotInitializeGmsInfo() {
        DeviceInfo.setGmsVersionCodeForTest("67890");

        assertEquals("67890", DeviceInfo.getGmsVersionCode());
        assertFalse(DeviceInfo.isGmsInfoInitializedForTesting());
    }

    @Test
    public void testGetSystemFeatureSnapshot() {
        PackageManager pm = mock(PackageManager.class);
        FeatureInfo unnamedFeature = new FeatureInfo();
        FeatureInfo vulkanFeature = createFeature(PackageManager.FEATURE_VULKAN_DEQP_LEVEL);
        vulkanFeature.version = 0x07E70301;
        when(pm.getSystemAvailableFeatures())
                .thenReturn(
                        new FeatureInfo[] {
                            null,
                            unnamedFeature,
                            createFeature(PackageManager.FEATURE_AUTOMOTIVE),
                            createFeature(PackageManager.FEATURE_PC), // nocheck
                            createFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE),
                            createFeature(DeviceInfo.XR_OPENXR_FEATURE_NAME),
                            vulkanFeature
                        });

        DeviceInfo.SystemFeatureSnapshot snapshot = DeviceInfo.getSystemFeatureSnapshot(pm);

        assertTrue(snapshot.mHasAutomotive);
        assertTrue(snapshot.mHasPc);
        assertTrue(snapshot.mHasHingeAngle);
        assertTrue(snapshot.mHasXr);
        assertEquals(0x07E70301, snapshot.mVulkanDeqpLevel);
    }

    @Test
    public void testGetSystemFeatureSnapshot_missingFeatures() {
        PackageManager pm = mock(PackageManager.class);
        when(pm.getSystemAvailableFeatures()).thenReturn(new FeatureInfo[0]);

        DeviceInfo.SystemFeatureSnapshot snapshot = DeviceInfo.getSystemFeatureSnapshot(pm);

        assertFalse(snapshot.mHasAutomotive);
        assertFalse(snapshot.mHasPc);
        assertFalse(snapshot.mHasHingeAngle);
        assertFalse(snapshot.mHasXr);
        assertEquals(0, snapshot.mVulkanDeqpLevel);
    }

    @Test
    public void testGetSystemFeatureSnapshot_nullFeatureList() {
        PackageManager pm = mock(PackageManager.class);
        when(pm.getSystemAvailableFeatures()).thenReturn(null);

        assertNull(DeviceInfo.getSystemFeatureSnapshot(pm));
    }

    @Test
    public void testGetSystemFeatureSnapshot_securityException() {
        PackageManager pm = mock(PackageManager.class);
        when(pm.getSystemAvailableFeatures()).thenThrow(new SecurityException());

        assertNull(DeviceInfo.getSystemFeatureSnapshot(pm));
    }

    private static FeatureInfo createFeature(String name) {
        FeatureInfo feature = new FeatureInfo();
        feature.name = name;
        return feature;
    }
}
