// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import android.content.Context;

import androidx.core.util.AtomicFile;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.url.GURL;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * This class provides methods to write/read most visited sites related info to devices.
 */
public class MostVisitedSitesMetadataUtils {
    private static final String TAG = "TopSites";
    /** Prevents two state directories from getting created simultaneously. */
    private static final Object DIR_CREATION_LOCK = new Object();

    /** Prevents two MostVisitedSitesUtils from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

    /** Current version of the cache, to be updated when the cache structure or meaning changes. */
    private static final int CACHE_VERSION = 1;

    private static File sStateDirectory;
    private static String sStateDirName = "top_sites";
    private static String sStateFileName = "top_sites";

    /**
     * Asynchronously serialize the suggestion lists and save it into the disk.
     * @param topSitesInfo Suggestion lists.
     * @param callback Callback function after saving file.
     */
    public static void saveSuggestionListsToFile(
            List<SiteSuggestion> topSitesInfo, Runnable callback) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                try {
                    byte[] listData = serializeTopSitesData(topSitesInfo);
                    saveSuggestionListsToFile(
                            getOrCreateTopSitesDirectory(), sStateFileName, listData);
                } catch (IOException e) {
                    Log.e(TAG, "Fail to save file.");
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                if (callback != null) {
                    callback.run();
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Restore the suggestion lists from the disk and deserialize them.
     * @return Suggestion lists
     * IOException: If there is any problem when restoring file or deserialize data, remove the
     * stale files and throw an exception, then the UI thread will know there is no cache file and
     * show something else.
     */
    public static List<SiteSuggestion> restoreFileToSuggestionLists() throws IOException {
        List<SiteSuggestion> suggestions;
        try {
            byte[] listData =
                    restoreFileToSuggestionLists(getOrCreateTopSitesDirectory(), sStateFileName);
            suggestions = deserializeTopSitesData(listData);
        } catch (IOException e) {
            getOrCreateTopSitesDirectory().delete();
            throw e;
        }
        return suggestions;
    }

    /**
     * Restore the suggestion lists from the disk and deserialize them on UI thread.
     * @return Suggestion lists
     * IOException: If there is any problem when restoring file or deserialize data, remove the
     * stale files and throw an exception, then the UI thread will know there is no cache file and
     * show something else.
     */
    public static List<SiteSuggestion> restoreFileToSuggestionListsOnUiThread() throws IOException {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return restoreFileToSuggestionLists();
        }
    }

    private static byte[] serializeTopSitesData(List<SiteSuggestion> topSitesInfo)
            throws IOException {
        int topSitesCount = topSitesInfo.size();

        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);

        // Save the count of the list of top sites to restore.
        stream.writeInt(topSitesCount);

        Log.d(TAG, "Serializing top sites lists; count: " + topSitesCount);

        // Save top sites.
        for (int i = 0; i < topSitesCount; i++) {
            stream.writeInt(CACHE_VERSION);
            stream.writeInt(topSitesInfo.get(i).faviconId);
            stream.writeUTF(topSitesInfo.get(i).title);
            stream.writeUTF(topSitesInfo.get(i).url.serialize());
            stream.writeUTF(topSitesInfo.get(i).allowlistIconPath);
            stream.writeInt(topSitesInfo.get(i).titleSource);
            stream.writeInt(topSitesInfo.get(i).source);
            stream.writeInt(topSitesInfo.get(i).sectionType);
            stream.writeLong(topSitesInfo.get(i).dataGenerationTime.getTime());
        }
        stream.close();
        Log.d(TAG, "Serializing top sites lists finished");
        return output.toByteArray();
    }

    private static List<SiteSuggestion> deserializeTopSitesData(byte[] listData)
            throws IOException {
        if (listData == null || listData.length == 0) {
            return null;
        }

        DataInputStream stream = new DataInputStream(new ByteArrayInputStream(listData));

        Log.d(TAG, "Deserializing top sites lists");

        Date dataGenerationTime;

        // Get how many top sites there are.
        final int count = stream.readInt();

        // Restore top sites.
        List<SiteSuggestion> suggestions = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            int version = stream.readInt();
            if (version > CACHE_VERSION) {
                throw new IOException("Cache version not supported.");
            }
            int faviconId = stream.readInt();
            String title = stream.readUTF();
            GURL url = GURL.deserialize(stream.readUTF());
            if (url.isEmpty()) throw new IOException("GURL deserialization failed.");

            String allowlistIconPath = stream.readUTF();
            int titleSource = stream.readInt();
            int source = stream.readInt();
            int sectionType = stream.readInt();
            dataGenerationTime = new Date(stream.readLong());
            SiteSuggestion newSite = new SiteSuggestion(title, url, allowlistIconPath, titleSource,
                    source, sectionType, dataGenerationTime);
            newSite.faviconId = faviconId;
            suggestions.add(newSite);
        }
        Log.d(TAG, "Deserializing top sites lists finished");
        return suggestions;
    }

    /**
     * Atomically writes the given serialized data out to disk.
     * @param stateDirectory Directory to save top sites data into.
     * @param stateFileName  File name to save top sites data into.
     * @param listData       Top sites data in the form of a serialized byte array.
     */
    private static void saveSuggestionListsToFile(
            File stateDirectory, String stateFileName, byte[] listData) {
        synchronized (SAVE_LIST_LOCK) {
            File metadataFile = new File(stateDirectory, stateFileName);
            AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream stream = null;
            try {
                stream = file.startWrite();
                stream.write(listData, 0, listData.length);
                file.finishWrite(stream);
                Log.i(TAG,
                        "Finished saving top sites list to file:" + metadataFile.getAbsolutePath());
            } catch (IOException e) {
                if (stream != null) file.failWrite(stream);
                Log.e(TAG, "Fail to write file: " + metadataFile.getAbsolutePath());
            }
        }
    }

    /**
     * Restore serialized data from disk.
     * @param stateDirectory Directory to save top sites data into.
     * @param stateFileName  File name to save top sites data into.
     * @return  Top sites data in the form of a serialized byte array.
     */
    private static byte[] restoreFileToSuggestionLists(File stateDirectory, String stateFileName)
            throws IOException {
        FileInputStream stream;
        byte[] data;

        File stateFile = new File(stateDirectory, stateFileName);
        stream = new FileInputStream(stateFile);
        data = new byte[(int) stateFile.length()];
        stream.read(data);
        Log.i(TAG, "Finished fetching top sites list.");

        StreamUtil.closeQuietly(stream);

        return data;
    }

    protected static File getOrCreateTopSitesDirectory() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            synchronized (DIR_CREATION_LOCK) {
                if (sStateDirectory == null) {
                    sStateDirectory = ContextUtils.getApplicationContext().getDir(
                            sStateDirName, Context.MODE_PRIVATE);
                }
            }
            return sStateDirectory;
        }
    }

    protected static File getStateDirectory() {
        return sStateDirectory;
    }
}
