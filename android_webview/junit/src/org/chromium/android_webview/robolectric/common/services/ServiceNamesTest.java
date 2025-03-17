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
                ServiceNames.AW_MINIDUMP_UPLOAD_JOB_SERVICE,
                AwMinidumpUploadJobService.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.CRASH_RECEIVER_SERVICE,
                CrashReceiverService.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.DEVELOPER_MODE_CONTENT_PROVIDER,
                DeveloperModeContentProvider.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.DEVELOPER_UI_SERVICE,
                DeveloperUiService.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.METRICS_BRIDGE_SERVICE,
                MetricsBridgeService.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.METRICS_UPLOAD_SERVICE,
                MetricsUploadService.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.VARIATIONS_SEED_SERVER,
                VariationsSeedServer.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                ServiceNames.AW_COMPONENT_UPDATE_SERVICE,
                AwComponentUpdateService.class.getName());
        Assert.assertEquals(
                "Incorrect class name constant",
                EmbeddedComponentLoader.AW_COMPONENTS_PROVIDER_SERVICE,
                ComponentsProviderService.class.getName());
    }
}
