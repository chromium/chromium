// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager.ChannelMigrationResult;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

/** Unit tests for {@link WebApkServiceClient} channel migration metrics. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.HIGH_PRIORITY_SITE_NOTIFICATIONS)
public class WebApkServiceClientTest {
    private static final String HISTOGRAM_NAME = "Notifications.Webapk.ChannelMigration.Result";
    private static final String TEST_PACKAGE = "org.chromium.webapk.test";

    @Before
    public void setUp() {
        WebappRegistry.refreshSharedPrefsForTesting();
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
    }

    private WebappDataStorage registerMockWebapp(String webApkPackage, int shellApkVersion) {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = webApkPackage;
        ApplicationInfo ai = new ApplicationInfo();
        ai.packageName = webApkPackage;
        ai.metaData = new Bundle();
        ai.metaData.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, shellApkVersion);
        packageInfo.applicationInfo = ai;
        shadowPackageManager.installPackage(packageInfo);

        String webappId = "webapp_id_" + webApkPackage;
        WebappRegistry.getInstance()
                .register(
                        webappId,
                        storage -> {
                            storage.updateWebApkPackageNameForTests(webApkPackage);
                        });
        RobolectricUtil.runAllBackgroundAndUi();

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorageForPackage(webApkPackage);
        assertNotNull(storage);
        return storage;
    }

    @Test
    public void testResolveChannelId_migratesToHighPriority() {
        WebappDataStorage storage = registerMockWebapp(TEST_PACKAGE, 192);
        storage.updateNotificationChannelId(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS);
        DeviceInfo.setIsDesktopForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NAME, ChannelMigrationResult.MIGRATED_TO_HIGH_PRIORITY);

        String channelId = WebApkServiceClient.resolveChannelIdAndRecordMigration(TEST_PACKAGE);

        assertEquals(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY, channelId);
        assertEquals(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY,
                storage.getNotificationChannelId());
        watcher.assertExpected();
    }

    @Test
    public void testResolveChannelId_downgradeToDefaultPriority() {
        WebappDataStorage storage = registerMockWebapp(TEST_PACKAGE, 192);
        storage.updateNotificationChannelId(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY);
        DeviceInfo.setIsDesktopForTesting(false);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NAME, ChannelMigrationResult.MIGRATED_TO_DEFAULT_PRIORITY);

        String channelId = WebApkServiceClient.resolveChannelIdAndRecordMigration(TEST_PACKAGE);

        assertEquals(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS, channelId);
        assertEquals(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS, storage.getNotificationChannelId());
        watcher.assertExpected();
    }

    @Test
    public void testResolveChannelId_noMigrationNeeded_alreadyHighPriority() {
        WebappDataStorage storage = registerMockWebapp(TEST_PACKAGE, 192);
        storage.updateNotificationChannelId(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY);
        DeviceInfo.setIsDesktopForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NAME, ChannelMigrationResult.NO_MIGRATION_NEEDED);

        String channelId = WebApkServiceClient.resolveChannelIdAndRecordMigration(TEST_PACKAGE);

        assertEquals(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY, channelId);
        assertEquals(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY,
                storage.getNotificationChannelId());
        watcher.assertExpected();
    }

    @Test
    public void testResolveChannelId_noMigrationNeeded_alreadyDefault() {
        WebappDataStorage storage = registerMockWebapp(TEST_PACKAGE, 192);
        storage.updateNotificationChannelId(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS);
        DeviceInfo.setIsDesktopForTesting(false);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NAME, ChannelMigrationResult.NO_MIGRATION_NEEDED);

        String channelId = WebApkServiceClient.resolveChannelIdAndRecordMigration(TEST_PACKAGE);

        assertEquals(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS, channelId);
        assertEquals(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS, storage.getNotificationChannelId());
        watcher.assertExpected();
    }

    @Test
    public void testResolveChannelId_legacyShellApkVersionDoesNotMigrate() {
        WebappDataStorage storage = registerMockWebapp(TEST_PACKAGE, 191);
        storage.updateNotificationChannelId(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS);
        DeviceInfo.setIsDesktopForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NAME, ChannelMigrationResult.NO_MIGRATION_NEEDED);

        String channelId = WebApkServiceClient.resolveChannelIdAndRecordMigration(TEST_PACKAGE);

        assertEquals(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS, channelId);
        assertEquals(
                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS, storage.getNotificationChannelId());
        watcher.assertExpected();
    }

    @Test
    public void testResolveChannelId_unregisteredPackageDoesNotCrash() {
        // Mock the package manager without registering storage in WebappRegistry.
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = "org.chromium.unregistered";
        ApplicationInfo ai = new ApplicationInfo();
        ai.packageName = "org.chromium.unregistered";
        ai.metaData = new Bundle();
        ai.metaData.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, 192);
        packageInfo.applicationInfo = ai;
        shadowPackageManager.installPackage(packageInfo);

        DeviceInfo.setIsDesktopForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(HISTOGRAM_NAME).build();

        String channelId =
                WebApkServiceClient.resolveChannelIdAndRecordMigration("org.chromium.unregistered");

        assertEquals(ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS_HIGH_PRIORITY, channelId);
        watcher.assertExpected();
    }
}
