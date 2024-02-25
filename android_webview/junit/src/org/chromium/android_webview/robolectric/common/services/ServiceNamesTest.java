// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.common.services;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.nonembedded.AwComponentUpdateService;
import org.chromium.android_webview.services.AwMinidumpUploadJobService;
import org.chromium.android_webview.services.ComponentsProviderService;
import org.chromium.android_webview.services.CrashReceiverService;
import org.chromium.android_webview.services.DeveloperModeContentProvider;
import org.chromium.android_webview.services.DeveloperUiService;
import org.chromium.android_webview.services.MetricsBridgeService;
import org.chromium.android_webview.services.MetricsUploadService;
import org.chromium.android_webview.services.VariationsSeedServer;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.component_updater.EmbeddedComponentLoader;

/** Tests the constants in ServiceNames. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ServiceNamesTest {
    @Test
    @SmallTest
    public void testServiceNamesValid() {
        Assert.assertEquals(
                "Incorrect class name constant",
                AwMinidumpUploadJobService.class.getName(),
                ServiceNames.AW_MINIDUMP_UPLOAD_JOB_SERVICE);
        Assert.assertEquals(
                "Incorrect class name constant",
                CrashReceiverService.class.getName(),
                ServiceNames.CRASH_RECEIVER_SERVICE);
        Assert.assertEquals(
                "Incorrect class name constant",
                DeveloperModeContentProvider.class.getName(),
                ServiceNames.DEVELOPER_MODE_CONTENT_PROVIDER);
        Assert.assertEquals(
                "Incorrect class name constant",
                DeveloperUiService.class.getName(),
                ServiceNames.DEVELOPER_UI_SERVICE);
        Assert.assertEquals(
                "Incorrect class name constant",
                MetricsBridgeService.class.getName(),
                ServiceNames.METRICS_BRIDGE_SERVICE);
        Assert.assertEquals(
                "Incorrect class name constant",
                MetricsUploadService.class.getName(),
                ServiceNames.METRICS_UPLOAD_SERVICE);
        Assert.assertEquals(
                "Incorrect class name constant",
                VariationsSeedServer.class.getName(),
                ServiceNames.VARIATIONS_SEED_SERVER);
        Assert.assertEquals(
                "Incorrect class name constant",
                AwComponentUpdateService.class.getName(),
                ServiceNames.AW_COMPONENT_UPDATE_SERVICE);
        Assert.assertEquals(
                "Incorrect class name constant",
                ComponentsProviderService.class.getName(),
                EmbeddedComponentLoader.AW_COMPONENTS_PROVIDER_SERVICE);
    }
}
