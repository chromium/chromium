// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.common.services;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.services.CrashReceiverService;
import org.chromium.android_webview.services.VariationsSeedServer;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests the constants in ServiceNames. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ServiceNamesTest {
    @Test
    @SmallTest
    public void testServiceNamesValid() {
        Assert.assertEquals("Incorrect class name constant", CrashReceiverService.class.getName(),
                ServiceNames.CRASH_RECEIVER_SERVICE);
        Assert.assertEquals("Incorrect class name constant", VariationsSeedServer.class.getName(),
                ServiceNames.VARIATIONS_SEED_SERVER);
    }
}
