// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelMetadata;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/** Utility methods and classes for testing home Surface. */
public class HomeSurfaceTestUtils {
    public static final String START_SURFACE_RETURN_TIME_IMMEDIATE =
            ChromeFeatureList.START_SURFACE_RETURN_TIME
                    + ":start_surface_return_time_on_tablet_seconds/0";

    private static final long MAX_TIMEOUT_MS = 30000L;

    /**
     * Wait for the tab state to be initialized.
     *
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForTabModel(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(
                cta.getTabModelSelector()::isTabStateInitialized,
                MAX_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Creates a Tab state metadata file without creating Tab state files for the given Tab's info.
     *
     * @param tabIds All the Tab IDs in the normal tab model.
     * @param urls All the Tab URLs in the normal tab model.
     * @param selectedIndex The selected index of normal tab model.
     */
    public static void prepareTabStateMetadataFile(
            int[] tabIds, @Nullable String[] urls, int selectedIndex) {
        TabModelMetadata normalInfo = new TabModelMetadata(selectedIndex);
        for (int i = 0; i < tabIds.length; i++) {
            normalInfo.ids.add(tabIds[i]);
            String url = urls != null ? urls[i] : "about:blank";
            normalInfo.urls.add(url);
        }
        TabModelMetadata incognitoInfo = new TabModelMetadata(0);

        TabModelSelectorMetadata selectorMetaData =
                new TabModelSelectorMetadata(normalInfo, incognitoInfo);

        TabPersistentStoreImpl.saveTabModelPrefs(0, ActiveTabState.OTHER);
        File metadataFile =
                new File(
                        TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                        TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(0));
        TabMetadataFileManager.saveListToFile(metadataFile, selectorMetaData);
    }

    /**
     * Create thumbnail bitmap of the tab based on the given id and write it to file.
     *
     * @param tabId The id of the target tab.
     * @param browserControlsStateProvider For getting the top offset.
     * @return The bitmap created.
     */
    public static Bitmap createThumbnailBitmapAndWriteToFile(
            int tabId, BrowserControlsStateProvider browserControlsStateProvider) {
        final int height = 100;
        final int width =
                Math.round(
                        height
                                * TabCardThemeUtil.getTabThumbnailAspectRatio(
                                        ContextUtils.getApplicationContext(),
                                        browserControlsStateProvider));
        final Bitmap thumbnailBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

        try {
            File thumbnailFile = TabContentManager.getTabThumbnailFileJpeg(tabId);
            if (thumbnailFile.exists()) {
                thumbnailFile.delete();
            }
            Assert.assertFalse(thumbnailFile.exists());

            FileOutputStream thumbnailFileOutputStream = new FileOutputStream(thumbnailFile);
            thumbnailBitmap.compress(Bitmap.CompressFormat.JPEG, 100, thumbnailFileOutputStream);
            thumbnailFileOutputStream.flush();
            thumbnailFileOutputStream.close();

            Assert.assertTrue(thumbnailFile.exists());
        } catch (IOException e) {
            e.printStackTrace();
        }
        return thumbnailBitmap;
    }

    /**
     * Gets the current active Tab from UI thread.
     *
     * @param cta The ChromeTabbedActivity under test.
     */
    public static Tab getCurrentTabFromUiThread(ChromeTabbedActivity cta) {
        AtomicReference<Tab> tab = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.set(TabModelUtils.getCurrentTab(cta.getCurrentTabModel())));
        return tab.get();
    }
}
