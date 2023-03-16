// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build.VERSION_CODES;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BuildInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BuildInfoTest {
    // These indexes match the values in the constructor of base/android/build_info.cc.
    private static final int IS_AT_LEAST_T = 26;
    private static final int IS_AUTOMOTIVE = 27;
    private static final int IS_AT_LEAST_U = 28;
    private static final int TARGETS_AT_LEAST_U = 29;
    private static final int SDK_CODENAME = 30;

    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mShadowPackageManager = Shadows.shadowOf(context.getPackageManager());
    }

    @Test
    public void testIsAutomotive_trueIfFeatureAutomotiveTrue() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);

        assertTrue(new BuildInfo().isAutomotive);
    }

    @Test
    public void testIsAutomotive_falseIfFeatureAutomotiveFalse() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);

        assertFalse(new BuildInfo().isAutomotive);
    }

    @Test
    public void testIsAutomotive_isTrue_setsGetAllPropertesTo1() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);
        String[] properties = new BuildInfo().getAllProperties();

        assertEquals("1", properties[IS_AUTOMOTIVE]);
    }

    @Test
    public void testIsAutomotive_isFalse_setsGetAllPropertesTo0() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);
        String[] properties = new BuildInfo().getAllProperties();

        assertEquals("0", properties[IS_AUTOMOTIVE]);
    }

    /**
     * TODO(donnd, https://crbug.com/1345962) Create useful tests for T and U.
     * This test hardly tests anything, so this is mostly a placeholder and sanity check for the
     * java constructor.
     * It would be better to add tests to the native BuildInfo for these releases and to check that
     * the native code and java code consistently interpret the array.
     */
    @Test
    @Config(sdk = VERSION_CODES.S_V2)
    public void testIsAtLeastX_OnS() {
        String[] properties = new BuildInfo().getAllProperties();

        assertEquals("0", properties[IS_AT_LEAST_T]);
        assertEquals("0", properties[IS_AT_LEAST_U]);
        assertEquals("REL", properties[SDK_CODENAME]);
        assertEquals("0", properties[TARGETS_AT_LEAST_U]);
    }
}
