// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import java.nio.channels.Channels;
import java.nio.channels.ClosedByInterruptException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;
import java.nio.channels.WritableByteChannel;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Optional;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;

/** Saves and restores {@link TabState} to and from files. */
public class TabStateFileManager {
    // Different variants will be experimented with and each variant will have
    // a different prefix.
    private static final String FLATBUFFER_PREFIX = "flatbufferv1_";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String SAVED_TAB_STATE_FILE_PREFIX = "tab";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO = "cryptonito";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX =
            FLATBUFFER_PREFIX + SAVED_TAB_STATE_FILE_PREFIX;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final String FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO =
            FLATBUFFER_PREFIX + SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO;

    private static final String TAG = "TabState";

    /** Checks if the TabState header is loaded properly. */
    protected static final long KEY_CHECKER = 0;

    /** Overrides the Chrome channel/package name to test a variant channel-specific behaviour. */
    private static String sChannelNameOverrideForTest;

    private static Deque<FlatBufferMigrationTask> sPendingFlatBufferMigrations = new ArrayDeque<>();
    private static List<FlatBufferMigrationTask> sExecutingFlatBufferMigrations =
            new LinkedList<>();

    private static boolean sDeferredStartupComplete;

    private static final int MAX_CONCURRENT_FLATBUFFER_MIGRATIONS = 1;

    /** Enum representing the exception that occurred during {@link restoreTabState}. */
    @IntDef({
        RestoreTabStateException.FILE_NOT_FOUND_EXCEPTION,
        RestoreTabStateException.CLOSED_BY_INTERRUPT_EXCEPTION,
        RestoreTabStateException.IO_EXCEPTION,
        RestoreTabStateException.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RestoreTabStateException {
        int FILE_NOT_FOUND_EXCEPTION = 0;
        int CLOSED_BY_INTERRUPT_EXCEPTION = 1;
        int IO_EXCEPTION = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * @param stateFolder folder {@link TabState} files are stored in
     * @param id {@link Tab} identifier
     * @return {@link TabState} corresponding to Tab with id
     */
    public static TabState restoreTabState(File stateFolder, int id) {
        // If the FlatBuffer schema is enabled, try to restore using that. There are no guarantees,
        // however - for example if the flag was just turned on there won't have been the
        // opportunity to save any FlatBuffer based {@link TabState} files yet. So we
        // always have a fallback to regular hand-written based TabState.
        if (isFlatBufferSchemaEnabled()) {
            TabState tabState = restoreTabState(stateFolder, id, true);
            if (tabState != null) {
                return tabState;
            }
        }
        // Flatbuffer flag is off or we couldn't restore the TabState using a FlatBuffer based
        // file e.g. file doesn't exist for the Tab or is corrupt.
        return restoreTabState(stateFolder, id, false);
    }

    /**
     * Restore a TabState file for a particular Tab. Checks if the Tab exists as a regular tab
     * before searching for an encrypted version.
     *
     * @param stateFolder Folder containing the TabState files.
     * @param id ID of the Tab to restore.
     * @param useFlatBuffer whether to restore using the FlatBuffer based TabState file or not.
     * @return TabState that has been restored, or null if it failed.
     */
    private static TabState restoreTabState(File stateFolder, int id, boolean useFlatBuffer) {
        // First try finding an unencrypted file.
        boolean encrypted = false;
        File file = getTabStateFile(stateFolder, id, encrypted, useFlatBuffer);

        // If that fails, try finding the encrypted version.
        if (!file.exists()) {
            encrypted = true;
            file = getTabStateFile(stateFolder, id, encrypted, useFlatBuffer);
        }

        // If they both failed, there's nothing to read.
        if (!file.exists()) return null;

        // If one of them passed, open the file input stream and read the state contents.
        long startTime = SystemClock.elapsedRealtime();
        TabState tabState = restoreTabStateInternal(file, encrypted);
        if (tabState != null) {
            RecordHistogram.recordTimesHistogram(
                    "Tabs.TabState.LoadTime", SystemClock.elapsedRealtime() - startTime);
        }
        return tabState;
    }

    /**
     * Restores a particular TabState file from storage.
     *
     * @param tabFile Location of the TabState file.
     * @param isEncrypted Whether the Tab state is encrypted or not.
     * @return TabState that has been restored, or null if it failed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static TabState restoreTabStateInternal(File tabFile, boolean isEncrypted) {
        TabState tabState = null;
        try {
            // TODO(b/307795775) investigate what strongly typed exceptions the FlatBuffer
            // code might throw and log metrics.
            tabState = readState(tabFile, isEncrypted);
        } catch (FileNotFoundException exception) {
            Log.e(TAG, "Failed to restore tab state for tab: " + tabFile);
            recordRestoreTabStateException(RestoreTabStateException.FILE_NOT_FOUND_EXCEPTION);
        } catch (ClosedByInterruptException exception) {
            Log.e(TAG, "Failed to restore tab state.", exception);
            recordRestoreTabStateException(RestoreTabStateException.CLOSED_BY_INTERRUPT_EXCEPTION);
        } catch (IOException exception) {
            Log.e(TAG, "Failed to restore tab state.", exception);
            recordRestoreTabStateException(RestoreTabStateException.IO_EXCEPTION);
        } catch (Throwable e) {
            if (tabFile.getName().startsWith(FLATBUFFER_PREFIX)) {
                // TODO(b/307597013) Record number of FlatBuffer Tab restoration success/failure
                // as well as hand-written based success/failure.
                String message =
                        String.format(
                                Locale.getDefault(),
                                "Error reading TabState file %s",
                                tabFile.getName());
                Log.i(TAG, message, e);
                assert false : message;

                // Catch all. FlatBuffer approach failed for some reason. Caller will know
                // to fall back to legacy TabState.
                return null;
            } else {
                throw e;
            }
        }
        return tabState;
    }

    private static void recordRestoreTabStateException(
            @RestoreTabStateException int restoreTabStateException) {
        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.RestoreTabStateException",
                restoreTabStateException,
                RestoreTabStateException.NUM_ENTRIES);
    }

    /**
     * Restores a particular TabState file from storage.
     *
     * @param file file with serialized {@link TabState}
     * @param encrypted Whether the file is encrypted or not.
     * @return TabState that has been restored, or null if it failed.
     */
    private static TabState readState(File file, boolean encrypted)
            throws IOException, FileNotFoundException {
        FileInputStream input = new FileInputStream(file);
        DataInputStream stream = null;
        try {
            if (encrypted) {
                Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.DECRYPT_MODE);
                if (cipher != null) {
                    stream = new DataInputStream(new CipherInputStream(input, cipher));
                }
            }
            if (stream == null) stream = new DataInputStream(input);
            if (encrypted && stream.readLong() != KEY_CHECKER) {
                // Got the wrong key, skip the file
                return null;
            }
            if (file.getName().startsWith(FLATBUFFER_PREFIX)) {
                FlatBufferTabStateSerializer serializer =
                        new FlatBufferTabStateSerializer(encrypted);
                if (encrypted) {
                    int size = stream.readInt();
                    byte[] res = new byte[size];
                    stream.readFully(res);
                    return serializer.deserialize(ByteBuffer.wrap(res));
                } else {
                    FileChannel channel = input.getChannel();
                    ByteBuffer res =
                            channel.map(MapMode.READ_ONLY, channel.position(), channel.size());
                    return serializer.deserialize(res);
                }
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
                tabState.contentsState =
                        new WebContentsState(
                                channel.map(MapMode.READ_ONLY, channel.position(), size));
                // Skip ahead to avoid re-reading data that mmap'd.
                long skipped = input.skip(size);
                if (skipped != size) {
                    Log.e(
                            TAG,
                            "Only skipped "
                                    + skipped
                                    + " bytes when "
                                    + size
                                    + " should've "
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
                Log.w(
                        TAG,
                        "Failed to read saved state version id from tab state. Assuming "
                                + "version "
                                + tabState.contentsState.version());
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
                Log.w(
                        TAG,
                        "Failed to read shouldPreserve flag from tab state. "
                                + "Assuming shouldPreserve is false");
            }
            tabState.isIncognito = encrypted;
            try {
                tabState.themeColor = stream.readInt();
            } catch (EOFException eof) {
                // Could happen if reading a version of TabState without a theme color.
                tabState.themeColor = TabState.UNSPECIFIED_THEME_COLOR;
                Log.w(
                        TAG,
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
                Log.w(
                        TAG,
                        "Failed to read tab launch type at creation from tab state. "
                                + "Assuming tab launch type is null");
            }
            try {
                tabState.rootId = stream.readInt();
            } catch (EOFException eof) {
                tabState.rootId = Tab.INVALID_TAB_ID;
                Log.w(
                        TAG,
                        "Failed to read tab root id from tab state. "
                                + "Assuming root id is Tab.INVALID_TAB_ID");
            }
            try {
                tabState.userAgent = stream.readInt();
            } catch (EOFException eof) {
                tabState.userAgent = TabUserAgent.UNSET;
                Log.w(
                        TAG,
                        "Failed to read tab user agent from tab state. "
                                + "Assuming user agent is TabUserAgent.UNSET");
            }
            try {
                tabState.lastNavigationCommittedTimestampMillis = stream.readLong();
            } catch (EOFException eof) {
                tabState.lastNavigationCommittedTimestampMillis = TabState.TIMESTAMP_NOT_SET;
                Log.w(
                        TAG,
                        "Failed to read last navigation committed timestamp from tab state."
                                + " Assuming last navigation committed timestamp is"
                                + " TabState.TIMESTAMP_NOT_SET");
            }
            // If FlatBuffer schema is enabled, but we restored using Legacy TabState, that
            // means the FlatBuffer file doesn't exist yet (e.g. Tab has gone uninteracted with
            // and there hasn't been an opportunity to migrate it to the FlatBuffer format).
            // So a migration should be initiated.
            if (isFlatBufferSchemaEnabled()) {
                Pair<Integer, Boolean> params = parseInfoFromFilename(file.getName());
                PostTask.runOrPostTask(
                        TaskTraits.UI_BEST_EFFORT,
                        () -> {
                            ThreadUtils.assertOnUiThread();
                            sPendingFlatBufferMigrations.add(
                                    new FlatBufferMigrationTask(
                                            /* tabId= */ params.first,
                                            /* isEncrypted= */ params.second,
                                            tabState,
                                            file.getParentFile()));
                            if (sDeferredStartupComplete) {
                                processNextFlatBufferMigration();
                            }
                        });
            }
            return tabState;
        } finally {
            StreamUtil.closeQuietly(stream);
            StreamUtil.closeQuietly(input);
        }
    }

    public static byte[] getContentStateByteArray(final ByteBuffer buffer) {
        byte[] contentsStateBytes = new byte[buffer.limit()];
        buffer.rewind();
        buffer.get(contentsStateBytes);
        return contentsStateBytes;
    }

    /**
     * @param directory directory TabState files are stored in
     * @param tabState TabState to store in a file
     * @param tabId identifier for the Tab
     * @param isEncrypted whether the stored Tab is encrypted or not
     */
    public static void saveState(
            File directory, TabState tabState, int tabId, boolean isEncrypted) {
        // Save regular hand-written based TabState file when the FlatBuffer flag is both on and
        // off.
        // We must always have a safe fallback to hand-written based TabState to be able to roll out
        // FlatBuffers safely.
        saveStateInternal(
                getTabStateFile(directory, tabId, isEncrypted, false), tabState, isEncrypted);
        if (isFlatBufferSchemaEnabled()) {
            saveStateInternal(
                    getTabStateFile(directory, tabId, isEncrypted, true), tabState, isEncrypted);
        }
    }

    /**
     * Writes the TabState to disk. This method may be called on either the UI or background thread.
     *
     * @param file File to write the tab's state to.
     * @param state State object obtained from from {@link Tab#getState()}.
     * @param encrypted Whether or not the TabState should be encrypted.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void saveStateInternal(File file, TabState state, boolean encrypted) {
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
                    dataOutputStream =
                            new DataOutputStream(
                                    new BufferedOutputStream(
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
            if (file.getName().contains(FLATBUFFER_PREFIX)) {
                try {
                    FlatBufferTabStateSerializer serializer =
                            new FlatBufferTabStateSerializer(encrypted);
                    ByteBuffer data = serializer.serialize(state);
                    if (encrypted) {
                        dataOutputStream.writeInt(data.remaining());
                    }
                    WritableByteChannel channel = Channels.newChannel(dataOutputStream);
                    channel.write(data);
                } catch (Throwable e) {
                    // Catch all in case of an issue saving the FlatBuffer file. Avoid crashing
                    // the app and simply log what went wrong.
                    Log.i(TAG, "Exception writing " + file.getName(), e);
                }
                return;
            }

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
            dataOutputStream.writeLong(state.lastNavigationCommittedTimestampMillis);
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
     *
     * @param directory Directory containing the TabState files.
     * @param tabId ID of the TabState to delete.
     * @param encrypted Whether the TabState is encrypted.
     * @param isFlatbuffer true if the TabState file is FlatBuffer schema based.
     * @return File corresponding to the given TabState.
     */
    public static File getTabStateFile(
            File directory, int tabId, boolean encrypted, boolean isFlatbuffer) {
        return new File(directory, getTabStateFilename(tabId, encrypted, isFlatbuffer));
    }

    /**
     * Deletes the TabState corresponding to the given Tab.
     * @param directory Directory containing the TabState files.
     * @param tabId ID of the TabState to delete.
     * @param encrypted Whether the TabState is encrypted.
     */
    public static void deleteTabState(File directory, int tabId, boolean encrypted) {
        for (boolean useFlatBuffer : new boolean[] {false, true}) {
            File file = getTabStateFile(directory, tabId, encrypted, useFlatBuffer);
            if (file.exists() && !file.delete()) Log.e(TAG, "Failed to delete TabState: " + file);
        }
    }

    /**
     * Generates the name of the state file that should represent the Tab specified by {@code id}
     * and {@code encrypted}.
     *
     * @param id The id of the {@link Tab} to save.
     * @param encrypted Whether or not the tab is incognito and should be encrypted.
     * @return The name of the file the Tab state should be saved to.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static String getTabStateFilename(int id, boolean encrypted, boolean isFlatBuffer) {
        if (isFlatBuffer) {
            return (encrypted
                            ? FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO
                            : FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX)
                    + id;
        }
        return (encrypted ? SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO : SAVED_TAB_STATE_FILE_PREFIX)
                + id;
    }

    /**
     * Delete TabState file asynchronously on a background thread.
     *
     * @param directory directory the TabState files are stored in.
     * @param tabId identifier for the Tab
     * @param encrypted True if the Tab is incognito.
     */
    public static void deleteAsync(File directory, int tabId, boolean encrypted) {
        PostTask.runOrPostTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    ThreadUtils.assertOnBackgroundThread();
                    deleteTabState(directory, tabId, encrypted);
                });
    }

    /**
     * Cleanup FlatBuffer files while the experiment is turned off. This ensures when the user
     * re-enters the FlatBuffer migration experiment we don't attempt to restore their Tabs using
     * out of date FlatBuffer files.
     *
     * @param stateDirectory directory where TabState files are saved.
     */
    public static void cleanupUnusedFiles(File stateDirectory) {
        if (isFlatBufferSchemaEnabled()) {
            return;
        }
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    ThreadUtils.assertOnBackgroundThread();
                    deleteFlatBufferFiles(stateDirectory);
                });
    }

    private static void deleteFlatBufferFiles(File stateDirectory) {
        for (File file : stateDirectory.listFiles()) {
            if (file.getName().startsWith(FLATBUFFER_PREFIX) && !file.delete()) {
                Log.e(TAG, "Failed to delete FlatBuffer TabState: " + file);
            }
        }
    }

    /**
     * Parse the tab id and whether the tab is incognito from the tab state filename.
     * @param name The given filename for the tab state file.
     * @return A {@link Pair} with tab id and incognito state read from the filename.
     */
    public static Pair<Integer, Boolean> parseInfoFromFilename(String name) {
        try {
            if (name.startsWith(SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO)) {
                int id =
                        Integer.parseInt(
                                name.substring(SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO.length()));
                return Pair.create(id, true);
            } else if (name.startsWith(SAVED_TAB_STATE_FILE_PREFIX)) {
                int id = Integer.parseInt(name.substring(SAVED_TAB_STATE_FILE_PREFIX.length()));
                return Pair.create(id, false);
            } else if (name.startsWith(FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO)) {
                int id =
                        Integer.parseInt(
                                name.substring(
                                        FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO.length()));
                return Pair.create(id, true);
            } else if (name.startsWith(FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX)) {
                int id =
                        Integer.parseInt(
                                name.substring(FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX.length()));
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
    public static void setChannelNameOverrideForTest(String name) {
        sChannelNameOverrideForTest = name;
        ResettersForTesting.register(() -> sChannelNameOverrideForTest = null);
    }

    private static boolean isFlatBufferSchemaEnabled() {
        return ChromeFeatureList.sTabStateFlatBuffer.isEnabled();
    }

    /***
     * Signal to {@link TabStateFileManager} that deferred startup has commenced.
     */
    public static void onDeferredStartup() {
        if (!isFlatBufferSchemaEnabled()) {
            return;
        }
        processNextFlatBufferMigration();
        sDeferredStartupComplete = true;
    }

    /***
     * Cancel migration of a {@link Tab} to FlatBuffer format (for example if a {@link Tab} is closed.
     * @param tabId identifier for a {@link Tab}
     * @param isEncrypted if a {@link Tab} is incognito or not.
     */
    public static void cancelMigration(int tabId, boolean isEncrypted) {
        if (!isFlatBufferSchemaEnabled()) {
            return;
        }
        Optional<FlatBufferMigrationTask> pendingFlatBufferMigrationTask =
                sPendingFlatBufferMigrations.stream()
                        .filter(f -> f.mTabId == tabId && f.mIsEncrypted == isEncrypted)
                        .findFirst();
        if (pendingFlatBufferMigrationTask.isPresent()) {
            sPendingFlatBufferMigrations.remove(pendingFlatBufferMigrationTask.get());
            return;
        }
        Optional<FlatBufferMigrationTask> sExecutingFlatBufferMigrationTask =
                sExecutingFlatBufferMigrations.stream()
                        .filter(f -> f.mTabId == tabId && f.mIsEncrypted == isEncrypted)
                        .findFirst();
        if (sExecutingFlatBufferMigrationTask.isPresent()) {
            sExecutingFlatBufferMigrationTask.get().cancel(false);
            sExecutingFlatBufferMigrations.remove(sExecutingFlatBufferMigrationTask.get());
            processNextFlatBufferMigration();
        }
    }

    private static class FlatBufferMigrationTask extends AsyncTask<Void> {
        protected int mTabId;
        protected boolean mIsEncrypted;
        protected TabState mTabState;
        protected File mStateDirectory;

        FlatBufferMigrationTask(
                int tabId, boolean isEncrypted, TabState tabState, File stateDirectory) {
            mTabId = tabId;
            mIsEncrypted = isEncrypted;
            mTabState = tabState;
            mStateDirectory = stateDirectory;
        }

        @Override
        protected Void doInBackground() {
            saveStateInternal(
                    getTabStateFile(
                            mStateDirectory, mTabId, mIsEncrypted, /* isFlatbuffer= */ true),
                    mTabState,
                    mIsEncrypted);
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (isCancelled()) {
                deleteOnCancel();
                return;
            }
            sExecutingFlatBufferMigrations.remove(this);
            processNextFlatBufferMigration();
        }

        private void deleteOnCancel() {
            PostTask.runOrPostTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> {
                        ThreadUtils.assertOnBackgroundThread();
                        File file =
                                getTabStateFile(
                                        mStateDirectory,
                                        mTabId,
                                        mIsEncrypted,
                                        /* isFlatbuffer= */ true);
                        if (file.exists() && !file.delete()) {
                            Log.e(TAG, "Failed to delete TabState: " + file);
                        }
                    });
        }
    }

    private static void processNextFlatBufferMigration() {
        if (sPendingFlatBufferMigrations.isEmpty()
                || sExecutingFlatBufferMigrations.size() >= MAX_CONCURRENT_FLATBUFFER_MIGRATIONS) {
            return;
        }
        FlatBufferMigrationTask nextFlatBufferMigrationTask =
                sPendingFlatBufferMigrations.removeFirst();
        sExecutingFlatBufferMigrations.add(nextFlatBufferMigrationTask);
        nextFlatBufferMigrationTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static boolean isFinishedFlatBufferMigration() {
        return sPendingFlatBufferMigrations.isEmpty() && sExecutingFlatBufferMigrations.isEmpty();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void resetDeferredStartupCompleteForTesting() {
        sDeferredStartupComplete = false;
    }
}
