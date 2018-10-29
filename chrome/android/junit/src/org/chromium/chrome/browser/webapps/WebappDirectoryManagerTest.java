// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.File;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Tests that directories for WebappActivities are managed correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class WebappDirectoryManagerTest {
    @Rule
    public MockWebappDataStorageClockRule mClockRule = new MockWebappDataStorageClockRule();

    private static final String WEBAPP_ID_1 = "webapp_1";
    private static final String WEBAPP_ID_2 = "webapp_2";
    private static final String WEBAPP_ID_3 = "webapp_3";
    private static final String WEBAPK_ID_1 = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_1";
    private static final String WEBAPK_ID_2 = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_2";
    private static final String WEBAPK_ID_3 = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_3";

    private static class TestWebappDirectoryManager extends WebappDirectoryManager {
        private Set<Intent> mBaseIntents = new HashSet<Intent>();

        @Override
        protected Set<Intent> getBaseIntentsForAllTasks() {
            return mBaseIntents;
        }
    }

    /** Deletes directory and all of its children. Recreates empty directory in its place. */
    private void deleteDirectoryAndRecreate(File f) {
        FileUtils.recursivelyDeleteFile(f);
        Assert.assertTrue(f.mkdirs());
    }

    private Context mContext;
    private TestWebappDirectoryManager mWebappDirectoryManager;

    @Before
    public void setUp() throws Exception {
        mContext = RuntimeEnvironment.application;
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        PathUtils.setPrivateDataDirectorySuffix("chrome");
        mWebappDirectoryManager = new TestWebappDirectoryManager();
        mWebappDirectoryManager.resetForTesting();

        // Set up directories.
        deleteDirectoryAndRecreate(mContext.getDataDir());
        FileUtils.recursivelyDeleteFile(mWebappDirectoryManager.getBaseWebappDirectory(mContext));
        deleteDirectoryAndRecreate(mContext.getCodeCacheDir());
    }

    @After
    public void tearDown() throws Exception {
        FileUtils.recursivelyDeleteFile(mContext.getDataDir());
        FileUtils.recursivelyDeleteFile(mContext.getCodeCacheDir());
        FileUtils.recursivelyDeleteFile(mWebappDirectoryManager.getBaseWebappDirectory(mContext));
        FileUtils.recursivelyDeleteFile(WebappDirectoryManager.getWebApkUpdateDirectory());
        ThreadUtils.setThreadAssertsDisabledForTesting(false);
    }

    public void registerWebapp(String webappId) {
        WebappRegistry.getInstance().register(
                webappId, new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {}
                });
        ShadowApplication.getInstance().runBackgroundTasks();
    }

    @Test
    @Feature({"Webapps"})
    public void testDeletesOwnDirectory() throws Exception {
        File webappDirectory =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPP_ID_1);
        Assert.assertTrue(webappDirectory.mkdirs());
        Assert.assertTrue(webappDirectory.exists());

        // Confirm that it deletes the current web app's directory.
        runCleanup();
        Assert.assertFalse(webappDirectory.exists());
    }

    /**
     * On Lollipop and higher, the {@link WebappDirectoryManager} also deletes directories for web
     * apps that no longer correspond to tasks in Recents.
     */
    @Test
    @Feature({"Webapps"})
    public void testDeletesDirectoriesForDeadTasks() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Track the three web app directories.
        File directory1 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPP_ID_1);
        File directory2 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPP_ID_2);
        File directory3 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPP_ID_3);

        // Seed the directory with folders for web apps.
        Assert.assertTrue(directory1.mkdirs());
        Assert.assertTrue(directory2.mkdirs());
        Assert.assertTrue(directory3.mkdirs());

        // Indicate that another of the web apps is listed in Recents; in real usage this web app
        // would not be in the foreground and would have persisted its state.
        mWebappDirectoryManager.mBaseIntents = new HashSet<Intent>();
        mWebappDirectoryManager.mBaseIntents.add(
                new Intent(Intent.ACTION_VIEW, Uri.parse("webapp://webapp_2")));

        // Only the directory for the background web app should survive.
        runCleanup();
        Assert.assertFalse(directory1.exists());
        Assert.assertTrue(directory2.exists());
        Assert.assertFalse(directory3.exists());
    }

    /**
     * On Lollipop and higher, the {@link WebappDirectoryManager} also deletes directories for
     * *WebApks* that no longer correspond to tasks in Recents.
     */
    @Test
    @Feature({"Webapps"})
    public void testDeletesDirectoriesForDeadWebApkTasks() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Track the three web app directories.
        File directory1 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPK_ID_1);
        File directory2 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPK_ID_2);
        File directory3 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mContext), WEBAPK_ID_3);

        // Seed the directory with folders for web apps.
        Assert.assertTrue(directory1.mkdirs());
        Assert.assertTrue(directory2.mkdirs());
        Assert.assertTrue(directory3.mkdirs());

        // Indicate that another of the web apps is listed in Recents; in real usage this web app
        // would not be in the foreground and would have persisted its state.
        mWebappDirectoryManager.mBaseIntents.add(
                new Intent(Intent.ACTION_VIEW, Uri.parse("webapp://webapk-webapp_2")));

        // Only the directory for the background web app should survive.
        runCleanup();
        Assert.assertFalse(directory1.exists());
        Assert.assertTrue(directory2.exists());
        Assert.assertFalse(directory3.exists());
    }

    @Test
    @Feature({"Webapps"})
    public void testDeletesObsoleteDirectories() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Seed the base directory with folders that correspond to pre-L web apps.
        File baseDirectory = mContext.getDataDir();
        File webappDirectory1 = new File(baseDirectory, "app_WebappActivity1");
        File webappDirectory6 = new File(baseDirectory, "app_WebappActivity6");
        File nonWebappDirectory = new File(baseDirectory, "app_ChromeDocumentActivity");
        Assert.assertTrue(webappDirectory1.mkdirs());
        Assert.assertTrue(webappDirectory6.mkdirs());
        Assert.assertTrue(nonWebappDirectory.mkdirs());

        // Make sure only the web app folders are deleted.
        runCleanup();
        Assert.assertFalse(webappDirectory1.exists());
        Assert.assertFalse(webappDirectory6.exists());
        Assert.assertTrue(nonWebappDirectory.exists());
    }

    /**
     * Test that WebApk.Update.NumStaleUpdateRequestFiles counts "update request" files for WebAPKs
     * which have been uninstalled.
     */
    @Test
    @Feature({"Webapps"})
    public void testCountsUpdateFilesForUninstalledWebApks() throws Exception {
        File directory1 = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_1);
        directory1.mkdirs();
        File directory2 = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_2);
        directory2.mkdirs();

        // No entry for WEBAPK_ID_1 and WEBAPK_ID_2 in WebappRegistry because the WebAPKs have been
        // uninstalled.

        runCleanup();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "WebApk.Update.NumStaleUpdateRequestFiles", 2));
    }

    /**
     * Test that WebApk.Update.NumStaleUpdateRequestFiles counts "update request" files for WebAPKs
     * for which an update was requested a long time ago.
     */
    @Test
    @Feature({"Webapps"})
    public void testCountsOldWebApkUpdateFiles() throws Exception {
        File directory = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_1);
        directory.mkdirs();
        registerWebapp(WEBAPK_ID_1);
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID_1);
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        mClockRule.advance(TimeUnit.DAYS.toMillis(30));

        runCleanup();
        Assert.assertEquals(1,
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
    public void testDoesNotCountFilesForNewlyScheduledUpdates() throws Exception {
        File directory = new File(WebappDirectoryManager.getWebApkUpdateDirectory(), WEBAPK_ID_1);
        directory.mkdirs();
        registerWebapp(WEBAPK_ID_1);
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID_1);
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        mClockRule.advance(1);

        runCleanup();
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "WebApk.Update.NumStaleUpdateRequestFiles", 1));
    }

    private void runCleanup() throws Exception {
        mWebappDirectoryManager.cleanUpDirectories(mContext, WEBAPP_ID_1);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
