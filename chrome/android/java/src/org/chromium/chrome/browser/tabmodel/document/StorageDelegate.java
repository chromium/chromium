// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.content.Context;
import android.util.SparseArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabPersister;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelInfo.DocumentEntry;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelInfo.DocumentList;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Contains functions for interacting with the file system.
 */
public class StorageDelegate extends TabPersister {
    private static final String TAG = "StorageDelegate";

    /** Filename to use for the DocumentTabModel that stores regular tabs. */
    private static final String REGULAR_FILE_NAME = "chrome_document_activity.store";

    /** Directory to store TabState files in. */
    private static final String STATE_DIRECTORY = "ChromeDocumentActivity";

    /** The buffer size to use when reading the DocumentTabModel file, set to 4k bytes. */
    private static final int BUF_SIZE = 0x1000;

    /** Cached base state directory to prevent main-thread filesystem access in getStateDirectory().
     */
    private static AsyncTask<File> sBaseStateDirectoryFetchTask;

    public StorageDelegate() {
        // Warm up the state directory to prevent it from using filesystem on main thread in the
        // future
        preloadStateDirectory();
    }

    /**
     * Reads the file containing the minimum info required to restore the state of the
     * {@link DocumentTabModel}.
     * @param encrypted Whether or not the file corresponds to an Incognito TabModel.
     * @return Byte buffer containing the task file's data, or null if it wasn't read.
     */
    protected byte[] readMetadataFileBytes(boolean encrypted) {
        // Incognito mode doesn't save its state out.
        if (encrypted) return null;

        // Read in the file.
        byte[] bytes = null;
        FileInputStream streamIn = null;
        try {
            String filename = getFilename(encrypted);
            streamIn = ContextUtils.getApplicationContext().openFileInput(filename);

            // Read the file from the file into the out stream.
            ByteArrayOutputStream streamOut = new ByteArrayOutputStream();
            byte[] buf = new byte[BUF_SIZE];
            int r;
            while ((r = streamIn.read(buf)) != -1) {
                streamOut.write(buf, 0, r);
            }
            bytes = streamOut.toByteArray();
        } catch (FileNotFoundException e) {
            Log.e(TAG, "DocumentTabModel file not found.");
        } catch (IOException e) {
            Log.e(TAG, "I/O exception", e);
        } finally {
            StreamUtil.closeQuietly(streamIn);
        }

        return bytes;
    }

    /**
     * Writes the file containing the minimum info required to restore the state of the
     * {@link DocumentTabModel}.
     * @param encrypted Whether the TabModel is incognito.
     * @param bytes Byte buffer containing the tab's data.
     */
    public void writeTaskFileBytes(boolean encrypted, byte[] bytes) {
        // Incognito mode doesn't save its state out.
        if (encrypted) return;

        FileOutputStream outputStream = null;
        try {
            outputStream = ContextUtils.getApplicationContext().openFileOutput(
                    getFilename(encrypted), Context.MODE_PRIVATE);
            outputStream.write(bytes);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "DocumentTabModel file not found", e);
        } catch (IOException e) {
            Log.e(TAG, "I/O exception", e);
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    private void preloadStateDirectory() {
        if (sBaseStateDirectoryFetchTask != null) return;

        sBaseStateDirectoryFetchTask = new AsyncTask<File>() {
            @Override
            protected File doInBackground() {
                return ContextUtils.getApplicationContext().getDir(
                        STATE_DIRECTORY, Context.MODE_PRIVATE);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** @return The directory that stores the TabState files. */
    @Override
    public File getStateDirectory() {
        try {
            return sBaseStateDirectoryFetchTask.get();
        } catch (InterruptedException e) {
        } catch (ExecutionException e) {
        }

        // If the AsyncTask failed for some reason, we have no choice but to fall back to
        // main-thread disk access.
        return ContextUtils.getApplicationContext().getDir(STATE_DIRECTORY, Context.MODE_PRIVATE);
    }

    /**
     * Restores the TabState with the given ID.
     * @param tabId ID of the Tab.
     * @return TabState for the Tab.
     */
    public TabState restoreTabState(int tabId, boolean encrypted) {
        return TabState.restoreTabState(getTabStateFile(tabId, encrypted), encrypted);
    }

    /**
     * Return the filename of the persisted TabModel state.
     * @param encrypted Whether or not the state belongs to an IncognitoDocumentTabModel.
     * @return String pointing at the TabModel's persisted state.
     */
    private String getFilename(boolean encrypted) {
        return encrypted ? null : REGULAR_FILE_NAME;
    }

    /**
     * Update tab entries based on metadata.
     * @param metadataBytes Metadata from last time Chrome was alive.
     * @param entryMap Map to fill with {@link DocumentTabModel.Entry}s about Tabs.
     * @param recentlyClosedTabIdList List to fill with IDs of recently closed tabs.
     */
    private void updateTabEntriesFromMetadata(byte[] metadataBytes, SparseArray<Entry> entryMap,
            List<Integer> recentlyClosedTabIdList) {
        if (metadataBytes != null) {
            DocumentList list = null;
            try {
                list = DocumentList.parseFrom(metadataBytes);
            } catch (IOException e) {
                Log.e(TAG, "I/O exception", e);
            }
            if (list == null) return;

            for (int i = 0; i < list.getEntriesCount(); i++) {
                DocumentEntry savedEntry = list.getEntries(i);
                int tabId = savedEntry.getTabId();

                // If the tab ID isn't in the list, it must have been closed after Chrome died.
                if (entryMap.indexOfKey(tabId) < 0) {
                    recentlyClosedTabIdList.add(tabId);
                    continue;
                }

                // Restore information about the Tab.
                entryMap.get(tabId).canGoBack = savedEntry.getCanGoBack();
            }
        }
    }

    /**
     * Constructs the DocumentTabModel's entries by combining the tasks currently listed in Android
     * with information stored out in a metadata file.
     * @param isIncognito               Whether to build an Incognito tab list.
     * @param activityDelegate          Interacts with the Activitymanager.
     * @param entryMap                  Map to fill with {@link DocumentTabModel.Entry}s about Tabs.
     * @param tabIdList                 List to fill with live Tab IDs.
     * @param recentlyClosedTabIdList   List to fill with IDs of recently closed tabs.
     */
    public void restoreTabEntries(final boolean isIncognito, ActivityDelegate activityDelegate,
            final SparseArray<Entry> entryMap, List<Integer> tabIdList,
            final List<Integer> recentlyClosedTabIdList) {
        assert entryMap.size() == 0;
        assert tabIdList.isEmpty();
        assert recentlyClosedTabIdList.isEmpty();

        // Run through Android's Overview to see what Chrome tabs are still listed.
        List<Entry> entries = activityDelegate.getTasksFromRecents(isIncognito);
        for (Entry entry : entries) {
            int tabId = entry.tabId;
            if (tabId != Tab.INVALID_TAB_ID) {
                if (!tabIdList.contains(tabId)) tabIdList.add(tabId);
                entryMap.put(tabId, entry);
            }

            // Prevent these tabs from being retargeted until we have had the opportunity to load
            // more information about them.
            entry.canGoBack = true;
        }

        new AsyncTask<byte[]>() {
            @Override
            protected byte[] doInBackground() {
                return readMetadataFileBytes(isIncognito);
            }

            @Override
            protected void onPostExecute(byte[] metadataBytes) {
                updateTabEntriesFromMetadata(metadataBytes, entryMap, recentlyClosedTabIdList);
            }
            // Run on serial executor to ensure that this is done before other start-up tasks.
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }
}
