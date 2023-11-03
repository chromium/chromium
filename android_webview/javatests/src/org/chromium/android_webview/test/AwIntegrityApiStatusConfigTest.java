// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwIntegrityApiStatusConfig;
import org.chromium.android_webview.AwIntegrityApiStatusConfig.ApiStatus;

import java.util.Map;

/** {@link org.chromium.android_webview.AwIntegrityApiStatusConfig} tests. */
@RunWith(AwJUnit4ClassRunner.class)
public class AwIntegrityApiStatusConfigTest {
    @Test
    @SmallTest
    public void testGetApiStatus_returnsEmptyConfig_whenNotSet() throws Throwable {
        AwIntegrityApiStatusConfig config = new AwIntegrityApiStatusConfig();

        Assert.assertEquals(ApiStatus.ENABLED, config.getDefaultStatus());
        Assert.assertTrue(config.getOverrideRules().isEmpty());
    }

    @Test
    @SmallTest
    public void testGetApiStatus_returnsConfig_whenSetWithRules() throws Throwable {
        AwIntegrityApiStatusConfig config = new AwIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
                Map.of("http://*.webview.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        config.setApiStatus(defaultPermission, overrideRules);

        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());
    }
}
