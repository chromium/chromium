// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.PackageManager;

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

        BuildInfo buildInfo = new BuildInfo();

        assertTrue(buildInfo.isAutomotive);
    }

    @Test
    public void testIsAutomotive_falseIfFeatureAutomotiveFalse() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);

        BuildInfo buildInfo = new BuildInfo();

        assertFalse(buildInfo.isAutomotive);
    }

    @Test
    public void testIsAutomotive_isTrue_setsGetAllPropertesTo1() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);

        BuildInfo buildInfo = new BuildInfo();
        String[] properties = buildInfo.getAllProperties();

        // This index matches the value in the constructor of base/android/build_info.cc.
        int isAutomotiveIndex = 27;

        assertEquals("1", properties[isAutomotiveIndex]);
    }

    @Test
    public void testIsAutomotive_isFalse_setsGetAllPropertesTo0() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);

        BuildInfo buildInfo = new BuildInfo();
        String[] properties = buildInfo.getAllProperties();

        // This index matches the value in the constructor of base/android/build_info.cc.
        int isAutomotiveIndex = 27;

        assertEquals("0", properties[isAutomotiveIndex]);
    }
}
