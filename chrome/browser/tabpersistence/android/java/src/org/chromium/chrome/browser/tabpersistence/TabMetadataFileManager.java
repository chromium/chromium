// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import android.util.SparseBooleanArray;

import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Saves and restores tab metadata to and from files. */
@NullMarked
public class TabMetadataFileManager {
    private static final String TAG = "TabMetadataFileManag";

    /**
     * The current version of the saved state file. Version 4: In addition to the tab's ID, save the
     * tab's last URL. Version 5: In addition to the total tab count, save the incognito tab count.
     */
    private static final int SAVED_STATE_VERSION = 5;

    /**
     * The prefix of the name of the file where the metadata is saved. Values returned by {@link
     * #getMetadataFileName(String)} must begin with this prefix.
     */
    @VisibleForTesting public static final String SAVED_METADATA_FILE_PREFIX = "tab_state";

    /** Prevents two TabPersistentStores from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

    /** Stores information about a TabModel. */
    public static class TabModelMetadata {
        public int index;
        public final List<Integer> ids;
        public final List<String> urls;

        @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
        public TabModelMetadata(int selectedIndex) {
            index = selectedIndex;
            ids = new ArrayList<>();
            urls = new ArrayList<>();
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof TabModelMetadata that)) return false;
            return index == that.index
                    && Objects.equals(ids, that.ids)
                    && Objects.equals(urls, that.urls);
        }

        @Override
        public int hashCode() {
            return Objects.hash(index, ids, urls);
        }
    }

    /** Stores meta data about the TabModelSelector which can be serialized to disk. */
    public static class TabModelSelectorMetadata {
        public final TabModelMetadata normalModelMetadata;
        public final TabModelMetadata incognitoModelMetadata;

        public TabModelSelectorMetadata(
                TabModelMetadata normalModelMetadata, TabModelMetadata incognitoModelMetadata) {
            this.normalModelMetadata = normalModelMetadata;
            this.incognitoModelMetadata = incognitoModelMetadata;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof TabModelSelectorMetadata that)) return false;
            return Objects.equals(normalModelMetadata, that.normalModelMetadata)
                    && Objects.equals(incognitoModelMetadata, that.incognitoModelMetadata);
        }

        @Override
        public int hashCode() {
            return Objects.hash(normalModelMetadata, incognitoModelMetadata);
        }
    }

    /** Callback interface to use while reading the persisted TabModelSelector info from disk. */
    public interface OnTabStateReadCallback {
        /**
         * To be called as the details about a persisted Tab are read from the TabModelSelector's
         * persisted data.
         *
         * @param index The index out of all tabs for the current tab read.
         * @param id The id for the current tab read.
         * @param url The url for the current tab read.
         * @param isIncognito Whether the Tab is definitely Incognito, or null if it couldn't be
         *     determined because we didn't know how many Incognito tabs were saved out.
         * @param isStandardActiveIndex Whether the current tab read is the normal active tab.
         * @param isIncognitoActiveIndex Whether the current tab read is the incognito active tab.
         */
        void onDetailsRead(
                int index,
                int id,
                String url,
                @Nullable Boolean isIncognito,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex);
    }

    /**
     * Extracts the tab information from a given tab state metadata stream.
     *
     * @param stream The stream pointing to the tab state metadata file to be parsed.
     * @param callback A callback to be streamed updates about the tab state information being read.
     * @param tabIds A mapping of tab ID to whether the tab is an off the record tab.
     * @return The next available tab ID based on the maximum ID referenced in this state file.
     */
    public static int readSavedMetadataFile(
            DataInputStream stream,
            @Nullable OnTabStateReadCallback callback,
            @Nullable SparseBooleanArray tabIds)
            throws IOException {
        if (stream == null) return 0;
        int nextId = 0;
        boolean skipUrlRead = false;
        boolean skipIncognitoCount = false;
        final int version = stream.readInt();
        if (version != SAVED_STATE_VERSION) {
            // We don't support restoring Tab data from before M18.
            if (version < 3) return 0;
            // Older versions are missing newer data.
            if (version < 5) skipIncognitoCount = true;
            if (version < 4) skipUrlRead = true;
        }

        final int count = stream.readInt();
        final int incognitoCount = skipIncognitoCount ? -1 : stream.readInt();
        final int incognitoActiveIndex = stream.readInt();
        int standardActiveIndex = stream.readInt();
        if (standardActiveIndex < incognitoCount) {
            // See https://crbug.com/354041918. This is equal to the original standard active index
            // + incognitoCount. If there are not standard tabs, that would be -1 + incognitoCount,
            // which unexpectedly maps to the last incognito tab. Adjust here.
            standardActiveIndex = TabModel.INVALID_TAB_INDEX;
        }
        if (count < 0 || incognitoActiveIndex >= count || standardActiveIndex >= count) {
            throw new IOException();
        }

        for (int i = 0; i < count; i++) {
            int id = stream.readInt();
            String tabUrl = skipUrlRead ? "" : stream.readUTF();
            if (id >= nextId) nextId = id + 1;
            if (tabIds != null) tabIds.append(id, true);

            Boolean isIncognito = (incognitoCount < 0) ? null : i < incognitoCount;

            if (callback != null) {
                callback.onDetailsRead(
                        i,
                        id,
                        tabUrl,
                        isIncognito,
                        i == standardActiveIndex,
                        i == incognitoActiveIndex);
            }
        }
        return nextId;
    }

    /**
     * Atomically writes the given tab model selector data to disk.
     *
     * @param metadataFile File to save TabModel data into.
     * @param metadata TabModel data in copied types.
     */
    public static void saveListToFile(File metadataFile, TabModelSelectorMetadata metadata) {
        synchronized (SAVE_LIST_LOCK) {
            androidx.core.util.AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream output = null;
            try {
                output = file.startWrite();

                int standardCount = metadata.normalModelMetadata.ids.size();
                int incognitoCount = metadata.incognitoModelMetadata.ids.size();

                // Determine how many Tabs there are.
                int numTabsTotal = incognitoCount + standardCount;
                Log.d(TAG, "Persisting tab lists; " + standardCount + ", " + incognitoCount);

                // Save the index file containing the list of tabs to restore. Wrap a
                // BufferedOutputStream to batch/buffer actual writes. Most urls are far smaller
                // than the 8K buffer.
                DataOutputStream stream = new DataOutputStream(new BufferedOutputStream(output));
                stream.writeInt(SAVED_STATE_VERSION);
                stream.writeInt(numTabsTotal);
                stream.writeInt(incognitoCount);
                stream.writeInt(metadata.incognitoModelMetadata.index);
                stream.writeInt(metadata.normalModelMetadata.index + incognitoCount);

                // Save incognito state first, so when we load, if the incognito files are
                // unreadable we can fall back easily onto the standard selected tab.
                for (int i = 0; i < incognitoCount; i++) {
                    stream.writeInt(metadata.incognitoModelMetadata.ids.get(i));
                    stream.writeUTF(metadata.incognitoModelMetadata.urls.get(i));
                }
                for (int i = 0; i < standardCount; i++) {
                    stream.writeInt(metadata.normalModelMetadata.ids.get(i));
                    stream.writeUTF(metadata.normalModelMetadata.urls.get(i));
                }

                stream.flush();
                file.finishWrite(output);

            } catch (IOException e) {
                if (output != null) file.failWrite(output);
            }
        }
    }

    /**
     * @param uniqueTag The tag that uniquely identifies this state file. Typically this is an index
     *     or ID.
     * @return The name of the state file.
     */
    public static String getMetadataFileName(String uniqueTag) {
        return SAVED_METADATA_FILE_PREFIX + uniqueTag;
    }

    /**
     * Parses the metadata file name and returns the unique tag encoded into it.
     *
     * @param metadataFileName The state file name to be parsed.
     * @return The unique tag used when generating the file name.
     */
    public static String getMetadataFileUniqueTag(String metadataFileName) {
        assert isMetadataFile(metadataFileName);
        return metadataFileName.substring(SAVED_METADATA_FILE_PREFIX.length());
    }

    /**
     * Returns whether the specified filename matches the expected pattern of the tab metadata
     * files.
     *
     * @param fileName The name of a file.
     * @return If the file name is a valid metadata file.
     */
    public static boolean isMetadataFile(String fileName) {
        // The .new/.bak suffixes may be added internally by AtomicFile before the file finishes
        // writing. Ignore files in this transitory state.
        return fileName.startsWith(SAVED_METADATA_FILE_PREFIX)
                && !fileName.endsWith(".new")
                && !fileName.endsWith(".bak");
    }
}
