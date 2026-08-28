// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric unit tests for {@link DeviceInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DeviceInfoUnitTest {
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
