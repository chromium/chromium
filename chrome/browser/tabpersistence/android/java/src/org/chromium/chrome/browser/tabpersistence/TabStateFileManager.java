// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.components.version_info.VersionInfo;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedByInterruptException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;

/**
 * Saves and restores {@link TabState} to and from files.
 */
public class TabStateFileManager {
    public static final String SAVED_TAB_STATE_FILE_PREFIX = "tab";
    public static final String SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO = "cryptonito";
    private static final String TAG = "TabState";
    /** Checks if the TabState header is loaded properly. */
    private static final long KEY_CHECKER = 0;
    /** Overrides the Chrome channel/package name to test a variant channel-specific behaviour. */
    private static String sChannelNameOverrideForTest;

    /**
     * Enum representing the exception that occurred during {@link restoreTabState}.
     */
    @IntDef({RestoreTabStateException.FILE_NOT_FOUND_EXCEPTION,
            RestoreTabStateException.CLOSED_BY_INTERRUPT_EXCEPTION,
            RestoreTabStateException.IO_EXCEPTION, RestoreTabStateException.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RestoreTabStateException {
        int FILE_NOT_FOUND_EXCEPTION = 0;
        int CLOSED_BY_INTERRUPT_EXCEPTION = 1;
        int IO_EXCEPTION = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Restore a TabState file for a particular Tab.  Checks if the Tab exists as a regular tab
     * before searching for an encrypted version.
     * @param stateFolder Folder containing the TabState files.
     * @param id ID of the Tab to restore.
     * @return TabState that has been restored, or null if it failed.
     */
    public static TabState restoreTabState(File stateFolder, int id) {
        // First try finding an unencrypted file.
        boolean encrypted = false;
        File file = getTabStateFile(stateFolder, id, encrypted);

        // If that fails, try finding the encrypted version.
        if (!file.exists()) {
            encrypted = true;
            file = getTabStateFile(stateFolder, id, encrypted);
        }

        // If they both failed, there's nothing to read.
        if (!file.exists()) return null;

        // If one of them passed, open the file input stream and read the state contents.
        long startTime = SystemClock.elapsedRealtime();
        TabState tabState = restoreTabState(file, encrypted);
        if (tabState != null) {
            RecordHistogram.recordTimesHistogram(
                    "Tabs.TabState.LoadTime", SystemClock.elapsedRealtime() - startTime);
        }
        return tabState;
    }

    /**
     * Restores a particular TabState file from storage.
     * @param tabFile Location of the TabState file.
     * @param isEncrypted Whether the Tab state is encrypted or not.
     * @return TabState that has been restored, or null if it failed.
     */
    public static TabState restoreTabState(File tabFile, boolean isEncrypted) {
        FileInputStream stream = null;
        TabState tabState = null;
        try {
            stream = new FileInputStream(tabFile);
            tabState = readState(stream, isEncrypted);
        } catch (FileNotFoundException exception) {
            Log.e(TAG, "Failed to restore tab state for tab: " + tabFile);
            recordRestoreTabStateException(RestoreTabStateException.FILE_NOT_FOUND_EXCEPTION);
        } catch (ClosedByInterruptException exception) {
            Log.e(TAG, "Failed to restore tab state.", exception);
            recordRestoreTabStateException(RestoreTabStateException.CLOSED_BY_INTERRUPT_EXCEPTION);
        } catch (IOException exception) {
            Log.e(TAG, "Failed to restore tab state.", exception);
            recordRestoreTabStateException(RestoreTabStateException.IO_EXCEPTION);
        } finally {
            StreamUtil.closeQuietly(stream);
        }
        return tabState;
    }

    private static void recordRestoreTabStateException(
            @RestoreTabStateException int restoreTabStateException) {
        RecordHistogram.recordEnumeratedHistogram("Tabs.RestoreTabStateException",
                restoreTabStateException, RestoreTabStateException.NUM_ENTRIES);
    }

    /**
     * Restores a particular TabState file from storage.
     * @param input Location of the TabState file.
     * @param encrypted Whether the file is encrypted or not.
     * @return TabState that has been restored, or null if it failed.
     */
    private static TabState readState(FileInputStream input, boolean encrypted) throws IOException {
        DataInputStream stream = null;
        if (encrypted) {
            Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.DECRYPT_MODE);
            if (cipher != null) {
                stream = new DataInputStream(new CipherInputStream(input, cipher));
            }
        }
        if (stream == null) stream = new DataInputStream(input);
        try {
            if (encrypted && stream.readLong() != KEY_CHECKER) {
                // Got the wrong key, skip the file
                return null;
            }
            TabState tabState = new TabState();
            tabState.timestampMillis = stream.readLong();
            int size = stream.readInt();
            if (encrypted) {
                // If it's encrypted, we have to read the stream normally to apply the cipher.
                byte[] state = new byte[size];
                stream.readFully(state);
                tabState.contentsState = new WebContentsState(ByteBuffer.allocateDirect(size));
                tabState.contentsState.buffer().put(state);
            } else {
                // If not, we can mmap the file directly, saving time and copies into the java heap.
                FileChannel channel = input.getChannel();
                tabState.contentsState = new WebContentsState(
                        channel.map(MapMode.READ_ONLY, channel.position(), size));
                // Skip ahead to avoid re-reading data that mmap'd.
                long skipped = input.skip(size);
                if (skipped != size) {
                    Log.e(TAG,
                            "Only skipped " + skipped + " bytes when " + size + " should've "
                                    + "been skipped. Tab restore may fail.");
                }
            }
            tabState.parentId = stream.readInt();
            try {
                tabState.openerAppId = stream.readUTF();
                if ("".equals(tabState.openerAppId)) tabState.openerAppId = null;
            } catch (EOFException eof) {
                // Could happen if reading a version of a TabState that does not include the app id.
                Log.w(TAG, "Failed to read opener app id state from tab state");
            }
            try {
                tabState.contentsState.setVersion(stream.readInt());
            } catch (EOFException eof) {
                // On the stable channel, the first release is version 18. For all other channels,
                // chrome 25 is the first release.
                tabState.contentsState.setVersion(isStableChannelBuild() ? 0 : 1);

                // Could happen if reading a version of a TabState that does not include the
                // version id.
                Log.w(TAG,
                        "Failed to read saved state version id from tab state. Assuming "
                                + "version " + tabState.contentsState.version());
            }
            try {
                // Skip obsolete sync ID.
                stream.readLong();
            } catch (EOFException eof) {
            }
            try {
                boolean shouldPreserveNotUsed = stream.readBoolean();
            } catch (EOFException eof) {
                // Could happen if reading a version of TabState without this flag set.
                Log.w(TAG,
                        "Failed to read shouldPreserve flag from tab state. "
                                + "Assuming shouldPreserve is false");
            }
            tabState.isIncognito = encrypted;
            try {
                tabState.themeColor = stream.readInt();
            } catch (EOFException eof) {
                // Could happen if reading a version of TabState without a theme color.
                tabState.themeColor = TabState.UNSPECIFIED_THEME_COLOR;
                Log.w(TAG,
                        "Failed to read theme color from tab state. "
                                + "Assuming theme color is TabState#UNSPECIFIED_THEME_COLOR");
            }
            try {
                tabState.tabLaunchTypeAtCreation = stream.readInt();
                if (tabState.tabLaunchTypeAtCreation < 0
                        || tabState.tabLaunchTypeAtCreation >= TabLaunchType.SIZE) {
                    tabState.tabLaunchTypeAtCreation = null;
                }
            } catch (EOFException eof) {
                tabState.tabLaunchTypeAtCreation = null;
                Log.w(TAG,
                        "Failed to read tab launch type at creation from tab state. "
                                + "Assuming tab launch type is null");
            }
            try {
                tabState.rootId = stream.readInt();
            } catch (EOFException eof) {
                tabState.rootId = Tab.INVALID_TAB_ID;
                Log.w(TAG,
                        "Failed to read tab root id from tab state. "
                                + "Assuming root id is Tab.INVALID_TAB_ID");
            }
            try {
                tabState.userAgent = stream.readInt();
            } catch (EOFException eof) {
                tabState.userAgent = TabUserAgent.UNSET;
                Log.w(TAG,
                        "Failed to read tab user agent from tab state. "
                                + "Assuming user agent is TabUserAgent.UNSET");
            }
            return tabState;
        } finally {
            stream.close();
        }
    }

    public static byte[] getContentStateByteArray(final ByteBuffer buffer) {
        byte[] contentsStateBytes = new byte[buffer.limit()];
        buffer.rewind();
        buffer.get(contentsStateBytes);
        return contentsStateBytes;
    }

    /**
     * Writes the TabState to disk. This method may be called on either the UI or background thread.
     * @param file File to write the tab's state to.
     * @param state State object obtained from from {@link Tab#getState()}.
     * @param encrypted Whether or not the TabState should be encrypted.
     */
    public static void saveState(File file, TabState state, boolean encrypted) {
        if (state == null || state.contentsState == null) return;
        long startTime = SystemClock.elapsedRealtime();

        // Create the byte array from contentsState before opening the FileOutputStream, in case
        // contentsState.buffer is an instance of MappedByteBuffer that is mapped to
        // the tab state file.
        // Use local ByteBuffer (backed by same byte[] to mitigate crbug.com/1297894)
        byte[] contentsStateBytes =
                getContentStateByteArray(state.contentsState.buffer().asReadOnlyBuffer());

        DataOutputStream dataOutputStream = null;
        FileOutputStream fileOutputStream = null;
        try {
            fileOutputStream = new FileOutputStream(file);

            if (encrypted) {
                Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.ENCRYPT_MODE);
                if (cipher != null) {
                    dataOutputStream = new DataOutputStream(new BufferedOutputStream(
                            new CipherOutputStream(fileOutputStream, cipher)));
                } else {
                    // If cipher is null, getRandomBytes failed, which means encryption is
                    // meaningless. Therefore, do not save anything. This will cause users
                    // to lose Incognito state in certain cases. That is annoying, but is
                    // better than failing to provide the guarantee of Incognito Mode.
                    return;
                }
            } else {
                dataOutputStream = new DataOutputStream(new BufferedOutputStream(fileOutputStream));
            }
            if (encrypted) dataOutputStream.writeLong(KEY_CHECKER);
            dataOutputStream.writeLong(state.timestampMillis);
            dataOutputStream.writeInt(contentsStateBytes.length);
            dataOutputStream.write(contentsStateBytes);
            dataOutputStream.writeInt(state.parentId);
            dataOutputStream.writeUTF(state.openerAppId != null ? state.openerAppId : "");
            dataOutputStream.writeInt(state.contentsState.version());
            dataOutputStream.writeLong(-1); // Obsolete sync ID.
            dataOutputStream.writeBoolean(false); // Obsolete attribute |SHOULD_PRESERVE|.
            dataOutputStream.writeInt(state.themeColor);
            dataOutputStream.writeInt(
                    state.tabLaunchTypeAtCreation != null ? state.tabLaunchTypeAtCreation : -1);
            dataOutputStream.writeInt(state.rootId);
            dataOutputStream.writeInt(state.userAgent);
            RecordHistogram.recordTimesHistogram(
                    "Tabs.TabState.SaveTime", SystemClock.elapsedRealtime() - startTime);
        } catch (FileNotFoundException e) {
            Log.w(TAG, "FileNotFoundException while attempting to save TabState.");
        } catch (IOException e) {
            Log.w(TAG, "IOException while attempting to save TabState.");
        } finally {
            StreamUtil.closeQuietly(dataOutputStream);
            StreamUtil.closeQuietly(fileOutputStream);
        }
    }

    /**
     * Returns a File corresponding to the given TabState.
     * @param directory Directory containing the TabState files.
     * @param tabId ID of the TabState to delete.
     * @param encrypted Whether the TabState is encrypted.
     * @return File corresponding to the given TabState.
     */
    public static File getTabStateFile(File directory, int tabId, boolean encrypted) {
        return new File(directory, getTabStateFilename(tabId, encrypted));
    }

    /**
     * Deletes the TabState corresponding to the given Tab.
     * @param directory Directory containing the TabState files.
     * @param tabId ID of the TabState to delete.
     * @param encrypted Whether the TabState is encrypted.
     */
    public static void deleteTabState(File directory, int tabId, boolean encrypted) {
        File file = getTabStateFile(directory, tabId, encrypted);
        if (file.exists() && !file.delete()) Log.e(TAG, "Failed to delete TabState: " + file);
    }

    /**
     * Generates the name of the state file that should represent the Tab specified by {@code id}
     * and {@code encrypted}.
     * @param id        The id of the {@link Tab} to save.
     * @param encrypted Whether or not the tab is incognito and should be encrypted.
     * @return          The name of the file the Tab state should be saved to.
     */
    public static String getTabStateFilename(int id, boolean encrypted) {
        return (encrypted ? SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO : SAVED_TAB_STATE_FILE_PREFIX)
                + id;
    }

    /**
     * Parse the tab id and whether the tab is incognito from the tab state filename.
     * @param name The given filename for the tab state file.
     * @return A {@link Pair} with tab id and incognito state read from the filename.
     */
    public static Pair<Integer, Boolean> parseInfoFromFilename(String name) {
        try {
            if (name.startsWith(SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO)) {
                int id = Integer.parseInt(
                        name.substring(SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO.length()));
                return Pair.create(id, true);
            } else if (name.startsWith(SAVED_TAB_STATE_FILE_PREFIX)) {
                int id = Integer.parseInt(name.substring(SAVED_TAB_STATE_FILE_PREFIX.length()));
                return Pair.create(id, false);
            }
        } catch (NumberFormatException ex) {
            // Expected for files not related to tab state.
        }
        return null;
    }

    /** @return Whether a Stable channel build of Chrome is being used. */
    private static boolean isStableChannelBuild() {
        if ("stable".equals(sChannelNameOverrideForTest)) return true;
        return VersionInfo.isStableBuild();
    }

    /**
     * Overrides the channel name for testing.
     * @param name Channel to use.
     */
    @VisibleForTesting
    public static void setChannelNameOverrideForTest(String name) {
        sChannelNameOverrideForTest = name;
    }
}
