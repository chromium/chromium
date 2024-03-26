// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import android.content.Context;

import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
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
import java.util.List;

/** This class provides methods to write/read most visited sites related info to devices. */
public class MostVisitedSitesMetadataUtils {
    private static final String TAG = "TopSites";

    /** The singleton helper class for this class. */
    private static class SingletonHelper {
        private static final MostVisitedSitesMetadataUtils INSTANCE =
                new MostVisitedSitesMetadataUtils();
    }

    /** Prevents two state directories from getting created simultaneously. */
    private static final Object DIR_CREATION_LOCK = new Object();

    /** Prevents two MostVisitedSitesUtils from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

    /** Current version of the cache, to be updated when the cache structure or meaning changes. */
    private static final int CACHE_VERSION = 1;

    private static File sStateDirectory;
    private static String sStateDirName = "top_sites";
    private static String sStateFileName = "top_sites";

    private Runnable mCurrentTask;
    private Runnable mPendingTask;

    private int mPendingTaskTilesNumForTesting;

    /**
     * @return The singleton instance.
     */
    public static MostVisitedSitesMetadataUtils getInstance() {
        return SingletonHelper.INSTANCE;
    }

    /**
     * Save new suggestion tiles to the disk. If there is already a task running, save this new
     * saving task as |mPendingTask|.
     * @param suggestionTiles The site suggestion tiles.
     */
    public void saveSuggestionListsToFile(List<Tile> suggestionTiles) {
        Runnable newTask =
                () -> saveSuggestionListsToFile(suggestionTiles, this::updatePendingToCurrent);

        if (mCurrentTask != null) {
            // Skip last mPendingTask which is not necessary to run.
            mPendingTask = newTask;
            mPendingTaskTilesNumForTesting = suggestionTiles.size();
        } else {
            // Assign newTask to mCurrentTask and run this task.
            mCurrentTask = newTask;
            // Skip any pending task.
            mPendingTask = null;

            Log.i(TAG, "Start a new task.");
            mCurrentTask.run();
        }
    }

    /**
     * Restore the suggestion lists from the disk and deserialize them.
     * @return Suggestion lists
     * IOException: If there is any problem when restoring file or deserialize data, remove the
     * stale files and throw an exception, then the UI thread will know there is no cache file and
     * show something else.
     */
    public static List<Tile> restoreFileToSuggestionLists() throws IOException {
        List<Tile> tiles;
        try {
            byte[] listData = restoreFileToBytes(getOrCreateTopSitesDirectory(), sStateFileName);
            tiles = deserializeTopSitesData(listData);
        } catch (IOException e) {
            getOrCreateTopSitesDirectory().delete();
            throw e;
        }
        return tiles;
    }

    /**
     * Restore the suggestion lists from the disk and deserialize them on UI thread.
     *
     * @return Suggestion lists IOException: If there is any problem when restoring file or
     *     deserialize data, remove the stale files and throw an exception, then the UI thread will
     *     know there is no cache file and show something else.
     */
    public static List<Tile> restoreFileToSuggestionListsOnUiThread() throws IOException {
        return restoreFileToSuggestionLists();
    }

    /**
     * Asynchronously serialize the suggestion lists and save it into the disk.
     *
     * @param suggestionTiles The site suggestion tiles.
     * @param callback Callback function after saving file.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void saveSuggestionListsToFile(List<Tile> suggestionTiles, Runnable callback) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                try {
                    byte[] listData = serializeTopSitesData(suggestionTiles);
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

    private static byte[] serializeTopSitesData(List<Tile> suggestionTiles) throws IOException {
        int topSitesCount = suggestionTiles.size();

        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);

        // Save the count of the list of top sites to restore.
        stream.writeInt(topSitesCount);

        // Save top sites.
        for (int i = 0; i < topSitesCount; i++) {
            stream.writeInt(CACHE_VERSION);
            stream.writeInt(suggestionTiles.get(i).getIndex());
            SiteSuggestion suggestionInfo = suggestionTiles.get(i).getData();
            stream.writeUTF(suggestionInfo.title);
            stream.writeUTF(suggestionInfo.url.serialize());
            // Write an empty string for the allowlistIconPath, which is a deprecated field.
            stream.writeUTF("");
            stream.writeInt(suggestionInfo.titleSource);
            stream.writeInt(suggestionInfo.source);
            stream.writeInt(suggestionInfo.sectionType);
        }
        stream.close();
        Log.i(TAG, "Serializing top sites lists finished; count: " + topSitesCount);
        return output.toByteArray();
    }

    private static List<Tile> deserializeTopSitesData(byte[] listData) throws IOException {
        if (listData == null || listData.length == 0) {
            return null;
        }

        DataInputStream stream = new DataInputStream(new ByteArrayInputStream(listData));

        // Get how many top sites there are.
        final int count = stream.readInt();

        // Restore top sites.
        List<Tile> tiles = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            int version = stream.readInt();
            if (version > CACHE_VERSION) {
                throw new IOException("Cache version not supported.");
            }
            int index = stream.readInt();
            String title = stream.readUTF();
            GURL url = GURL.deserialize(stream.readUTF());
            if (url.isEmpty()) throw new IOException("GURL deserialization failed.");

            // Read the allowlistIconPath, which is always an empty string.
            String allowlistIconPath = stream.readUTF();
            int titleSource = stream.readInt();
            int source = stream.readInt();
            int sectionType = stream.readInt();
            SiteSuggestion newSite =
                    new SiteSuggestion(title, url, titleSource, source, sectionType);
            Tile newTile = new Tile(newSite, index);
            tiles.add(newTile);
        }
        Log.i(TAG, "Deserializing top sites lists finished");
        return tiles;
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
                Log.i(
                        TAG,
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
    private static byte[] restoreFileToBytes(File stateDirectory, String stateFileName)
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static File getOrCreateTopSitesDirectory() {
        synchronized (DIR_CREATION_LOCK) {
            if (sStateDirectory == null) {
                sStateDirectory =
                        ContextUtils.getApplicationContext()
                                .getDir(sStateDirName, Context.MODE_PRIVATE);
            }
        }
        return sStateDirectory;
    }

    private void updatePendingToCurrent() {
        mCurrentTask = mPendingTask;
        mPendingTask = null;
        if (mCurrentTask != null) {
            Log.i(TAG, "Start a new task.");
            mCurrentTask.run();
        }
    }

    public Runnable getCurrentTaskForTesting() {
        return mCurrentTask;
    }

    public void setCurrentTaskForTesting(Runnable currentTask) {
        var oldValue = mCurrentTask;
        mCurrentTask = currentTask;
        ResettersForTesting.register(() -> mCurrentTask = oldValue);
    }

    public void setPendingTaskForTesting(Runnable pendingTask) {
        var oldValue = mPendingTask;
        mPendingTask = pendingTask;
        ResettersForTesting.register(() -> mPendingTask = oldValue);
    }

    public int getPendingTaskTilesNumForTesting() {
        return mPendingTaskTilesNumForTesting;
    }
}
