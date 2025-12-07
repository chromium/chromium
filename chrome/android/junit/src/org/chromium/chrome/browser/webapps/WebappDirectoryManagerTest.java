// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.File;
import java.util.concurrent.TimeUnit;

/** Tests that directories for WebappActivities are managed correctly. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class WebappDirectoryManagerTest {
    @Rule public FakeTimeTestRule mClockRule = new FakeTimeTestRule();

    private static final String WEBAPK_PACKAGE_NAME_1 = "webapk_1";
    private static final String WEBAPK_PACKAGE_NAME_2 = "webapk_2";
    private static final String WEBAPK_ID_1 =
            WebApkConstants.WEBAPK_ID_PREFIX + WEBAPK_PACKAGE_NAME_1;
    private static final String WEBAPK_ID_2 =
            WebApkConstants.WEBAPK_ID_PREFIX + WEBAPK_PACKAGE_NAME_2;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        ContextUtils.initApplicationContext(mContext);
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        PathUtils.setPrivateDataDirectorySuffix("chrome");
        WebappDirectoryManager.resetForTesting();
    }

    public void registerWebapp(String webappId) {
        WebappRegistry.getInstance()
                .register(
                        webappId,
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {}
                        });
        ShadowApplication.runBackgroundTasks();
    }

    @Test
    @Feature({"Webapps"})
    public void testDeletesObsoleteDirectories() {
        // Seed the base directory with folders that correspond to pre-L web apps.
        File baseDirectory = mContext.getDataDir();
        File webappDirectory = new File(baseDirectory, "app_WebappActivity");
        File nonWebappDirectory = new File(baseDirectory, "app_ChromeDocumentActivity");
        Assert.assertTrue(webappDirectory.mkdirs());
        Assert.assertTrue(nonWebappDirectory.mkdirs());

        // Make sure only the web app folders are deleted.
        runCleanup();
        Assert.assertFalse(webappDirectory.exists());
        Assert.assertTrue(nonWebappDirectory.exists());

        Assert.assertTrue(webappDirectory.mkdirs());
        // Make sure the second cleanup call no-ops.
        runCleanup();
        Assert.assertTrue(webappDirectory.exists());
        Assert.assertTrue(nonWebappDirectory.exists());
    }

    /**
     * Test that WebApk.Update.NumStaleUpdateRequestFiles counts "update request" files for WebAPKs
     * which have been uninstalled.
     */
    @Test
    @Feature({"Webapps"})
    public void testCountsUpdateFilesForUninstalledWebApks() {
        File directory1 = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_1);
        directory1.mkdirs();
        File directory2 = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_2);
        directory2.mkdirs();

        // No entry for WEBAPK_ID_1 and WEBAPK_ID_2 in WebappRegistry because the WebAPKs have been
        // uninstalled.

        runCleanup();
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "WebApk.Update.NumStaleUpdateRequestFiles", 2));
    }

    /**
     * Test that WebApk.Update.NumStaleUpdateRequestFiles counts "update request" files for WebAPKs
     * for which an update was requested a long time ago.
     */
    @Test
    @Feature({"Webapps"})
    public void testCountsOldWebApkUpdateFiles() {
        File directory = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_1);
        directory.mkdirs();
        registerWebapp(WEBAPK_ID_1);
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID_1);
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        mClockRule.advanceMillis(TimeUnit.DAYS.toMillis(30));

        runCleanup();
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "WebApk.Update.NumStaleUpdateRequestFiles", 1));
    }

    /**
     * Test that WebApk.Update.NumStaleUpdateRequestFiles does not count "update request" files for
     * WebAPKs for which an update was recently requested. There is a 1-23 hour delay between
     * a WebAPK update being scheduled to the WebAPK being updated.
     */
    @Test
    @Feature({"Webapps"})
    public void testDoesNotCountFilesForNewlyScheduledUpdates() {
        File directory = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_1);
        directory.mkdirs();
        registerWebapp(WEBAPK_ID_1);
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID_1);
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        mClockRule.advanceMillis(1);

        runCleanup();
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "WebApk.Update.NumStaleUpdateRequestFiles", 1));
    }

    private void runCleanup() {
        WebappDirectoryManager.cleanUpDirectories();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
