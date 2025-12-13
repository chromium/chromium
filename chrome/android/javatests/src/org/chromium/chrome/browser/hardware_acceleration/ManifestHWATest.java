// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PackageUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.app.ChromeActivity;

/** Hardware acceleration-related manifest tests. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@DisableIf.Build(
        sdk_is_less_than = Build.VERSION_CODES.TIRAMISU,
        supported_abis_includes = "x86_64",
        message = "vr tests do not apply to emulator")
public class ManifestHWATest {
    @Test
    @SmallTest
    public void testAccelerationDisabled() throws Exception {
        PackageInfo info = PackageUtils.getApplicationPackageInfo(PackageManager.GET_ACTIVITIES);
        for (ActivityInfo activityInfo : info.activities) {
            String activityName =
                    activityInfo.targetActivity != null
                            ? activityInfo.targetActivity
                            : activityInfo.name;
            try {
                Class<?> activityClass = Class.forName(activityName);
                if (ChromeActivity.class.isAssignableFrom(activityClass)) {
                    // Every activity derived from ChromeActivity must disable hardware
                    // acceleration in the manifest.
                    Assert.assertTrue(
                            0 == (activityInfo.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED));
                }
            } catch (ClassNotFoundException e) {
                // Some test-only manifest entries exist only to test intent behavior and do not
                // represent real Activities (and should never be launched).
            }
        }
    }
}
