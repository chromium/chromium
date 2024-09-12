// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.M26_GOOGLE_COM;

import android.content.Intent;
import android.graphics.Bitmap;
import android.util.Base64;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/** Utility methods and classes for testing home Surface. */
public class HomeSurfaceTestUtils {
    public static final String IMMEDIATE_RETURN_TEST_PARAMS =
            "force-fieldtrial-params=Study.Group:"
                    + ReturnToChromeUtil.HOME_SURFACE_RETURN_TIME_SECONDS_PARAM
                    + "/0";

    private static final long MAX_TIMEOUT_MS = 30000L;

    /**
     * Only launch Chrome without waiting for a current tab. This method could not use {@link
     * ChromeTabbedActivityTestRule#startMainActivityFromLauncher()} because of its {@link
     * org.chromium.chrome.browser.tab.Tab} dependency.
     */
    public static void startMainActivityFromLauncher(ChromeActivityTestRule activityTestRule) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityTestRule.prepareUrlIntent(intent, null);
        activityTestRule.launchActivity(intent);
    }

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
     * Create all the files so that tab models can be restored.
     *
     * @param tabIds all the Tab IDs in the normal tab model.
     */
    public static void createTabStatesAndMetadataFile(int[] tabIds) throws IOException {
        createTabStatesAndMetadataFile(tabIds, null, null, 0);
    }

    /**
     * Create all the files so that tab models can be restored.
     *
     * @param tabIds all the Tab IDs in the normal tab model.
     * @param rootIds all the root IDs in the normal tab model.
     */
    public static void createTabStatesAndMetadataFile(int[] tabIds, @Nullable int[] rootIds)
            throws IOException {
        createTabStatesAndMetadataFile(tabIds, rootIds, null, 0);
    }

    /**
     * Create all the files so that tab models can be restored.
     *
     * @param tabIds all the Tab IDs in the normal tab model.
     * @param rootIds all the root IDs in the normal tab model.
     * @param urls all of the URLs in the normal tab model.
     * @param selectedIndex the selected index of normal tab model.
     */
    public static void createTabStatesAndMetadataFile(
            int[] tabIds, @Nullable int[] rootIds, @Nullable String[] urls, int selectedIndex)
            throws IOException {
        createTabStatesAndMetadataFile(tabIds, rootIds, urls, selectedIndex, true);
    }

    private static void createTabStatesAndMetadataFile(
            int[] tabIds,
            int[] rootIds,
            @Nullable String[] urls,
            int selectedIndex,
            boolean createStateFile)
            throws IOException {
        TabPersistentStore.TabModelMetadata normalInfo =
                new TabPersistentStore.TabModelMetadata(selectedIndex);
        for (int i = 0; i < tabIds.length; i++) {
            normalInfo.ids.add(tabIds[i]);
            String url = urls != null ? urls[i] : "about:blank";
            normalInfo.urls.add(url);

            if (createStateFile) {
                int rootId = rootIds == null ? tabIds[i] : rootIds[i];
                saveTabState(tabIds[i], rootId);
            }
        }
        TabPersistentStore.TabModelMetadata incognitoInfo =
                new TabPersistentStore.TabModelMetadata(0);

        TabPersistentStore.TabModelSelectorMetadata selectorMetaData =
                new TabPersistentStore.TabModelSelectorMetadata(normalInfo, incognitoInfo);

        TabPersistentStore.saveTabModelPrefs(normalInfo, incognitoInfo, 0, ActiveTabState.OTHER);
        File metadataFile =
                new File(
                        TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                        TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(0));
        TabPersistentStore.saveListToFile(metadataFile, selectorMetaData);
    }

    /**
     * Creates a Tab state metadata file without creating Tab state files for the given Tab's info.
     *
     * @param tabIds All the Tab IDs in the normal tab model.
     * @param urls All the Tab URLs in the normal tab model.
     * @param selectedIndex The selected index of normal tab model.
     */
    public static void prepareTabStateMetadataFile(
            int[] tabIds, @Nullable String[] urls, int selectedIndex) throws IOException {
        createTabStatesAndMetadataFile(tabIds, null, urls, selectedIndex, false);
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
                (int)
                        Math.round(
                                height
                                        * TabUtils.getTabThumbnailAspectRatio(
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
    public static Tab getCurrentTabFromUIThread(ChromeTabbedActivity cta) {
        AtomicReference<Tab> tab = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.set(TabModelUtils.getCurrentTab(cta.getCurrentTabModel())));
        return tab.get();
    }

    /**
     * Create a file so that a TabState can be restored later.
     *
     * @param tabId the Tab ID
     * @param rootId the Root ID
     */
    private static void saveTabState(int tabId, int rootId) {
        File file =
                TabStateFileManager.getTabStateFile(
                        TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                        tabId,
                        /* encrypted= */ false,
                        /* isFlatBuffer= */ false);
        writeFile(file, M26_GOOGLE_COM.encodedTabState);

        CipherFactory unusedCipherFactory = new CipherFactory();
        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(
                        file, /* encrypted= */ false, unusedCipherFactory);
        tabState.rootId = rootId;
        TabStateFileManager.saveStateInternal(
                file, tabState, /* encrypted= */ false, unusedCipherFactory);
    }

    private static void writeFile(File file, String data) {
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(Base64.decode(data, 0));
        } catch (Exception e) {
            assert false : "Failed to create " + file;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }
}
