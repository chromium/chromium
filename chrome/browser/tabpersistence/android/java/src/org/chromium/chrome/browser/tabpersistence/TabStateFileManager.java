// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import android.os.SystemClock;
import android.util.AtomicFile;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.ClosedByInterruptException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;
import java.nio.channels.WritableByteChannel;
import java.util.Locale;

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

    private static final long NO_TAB_GROUP_ID = 0L;

    public static final BooleanCachedFieldTrialParameter MIGRATE_STALE_TABS_CACHED_PARAM =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_STATE_FLAT_BUFFER, "migrate_stale_tabs", false);

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

    @IntDef({
        TabStateRestoreMethod.FLATBUFFER,
        TabStateRestoreMethod.LEGACY_HAND_WRITTEN,
        TabStateRestoreMethod.FAILED,
        TabStateRestoreMethod.NUM_ENTRIES,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public @interface TabStateRestoreMethod {
        /** TabState restored using FlatBuffer schema */
        int FLATBUFFER = 0;

        /** TabState restored using Legacy Handwritten schema */
        int LEGACY_HAND_WRITTEN = 1;

        /** TabState failed to be restored. */
        int FAILED = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * @param stateFolder folder {@link TabState} files are stored in
     * @param id {@link Tab} identifier
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     * @return {@link TabState} corresponding to Tab with id
     */
    public static TabState restoreTabState(File stateFolder, int id, CipherFactory cipherFactory) {
        // If the FlatBuffer schema is enabled, try to restore using that. There are no guarantees,
        // however - for example if the flag was just turned on there won't have been the
        // opportunity to save any FlatBuffer based {@link TabState} files yet. So we
        // always have a fallback to regular hand-written based TabState.
        if (isFlatBufferSchemaEnabled()) {
            TabState tabState = null;
            try {
                tabState = restoreTabState(stateFolder, id, cipherFactory, true);
            } catch (Exception e) {
                // TODO(crbug.com/341122002) Add in metrics
                Log.d(TAG, "Error restoring TabState using FlatBuffer", e);
            }
            if (tabState != null) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Tabs.TabState.RestoreMethod",
                        TabStateRestoreMethod.FLATBUFFER,
                        TabStateRestoreMethod.NUM_ENTRIES);
                return tabState;
            }
        }
        // Flatbuffer flag is off or we couldn't restore the TabState using a FlatBuffer based
        // file e.g. file doesn't exist for the Tab or is corrupt.
        TabState tabState = restoreTabState(stateFolder, id, cipherFactory, false);
        if (tabState == null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.TabState.RestoreMethod",
                    TabStateRestoreMethod.FAILED,
                    TabStateRestoreMethod.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.TabState.RestoreMethod",
                    TabStateRestoreMethod.LEGACY_HAND_WRITTEN,
                    TabStateRestoreMethod.NUM_ENTRIES);
        }
        return tabState;
    }

    /**
     * Restore a TabState file for a particular Tab. Checks if the Tab exists as a regular tab
     * before searching for an encrypted version.
     *
     * @param stateFolder Folder containing the TabState files.
     * @param id ID of the Tab to restore.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     * @param useFlatBuffer whether to restore using the FlatBuffer based TabState file or not.
     * @return TabState that has been restored, or null if it failed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static TabState restoreTabState(
            File stateFolder, int id, CipherFactory cipherFactory, boolean useFlatBuffer) {
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
        TabState tabState = restoreTabStateInternal(file, encrypted, cipherFactory);
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
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     * @return TabState that has been restored, or null if it failed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static TabState restoreTabStateInternal(
            File tabFile, boolean isEncrypted, CipherFactory cipherFactory) {
        TabState tabState = null;
        try {
            // TODO(b/307795775) investigate what strongly typed exceptions the FlatBuffer
            // code might throw and log metrics.
            tabState = readState(tabFile, isEncrypted, cipherFactory);
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
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     * @return TabState that has been restored, or null if it failed.
     */
    private static TabState readState(File file, boolean encrypted, CipherFactory cipherFactory)
            throws IOException, FileNotFoundException {
        if (file.getName().startsWith(FLATBUFFER_PREFIX)) {
            return readStateFlatBuffer(file, encrypted, cipherFactory);
        }
        FileInputStream input = new FileInputStream(file);
        DataInputStream stream = null;
        try {
            if (encrypted) {
                Cipher cipher = cipherFactory.getCipher(Cipher.DECRYPT_MODE);
                if (cipher != null) {
                    stream = new DataInputStream(new CipherInputStream(input, cipher));
                }
            }
            if (stream == null) stream = new DataInputStream(input);
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
                // Skip obsolete shouldPreserve.
                stream.readBoolean();
            } catch (EOFException eof) {
                // Could happen if reading a version of TabState without this flag set.
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
                    tabState.tabLaunchTypeAtCreation = TabLaunchType.UNSET;
                }
            } catch (EOFException eof) {
                tabState.tabLaunchTypeAtCreation = TabLaunchType.UNSET;
                Log.w(
                        TAG,
                        "Failed to read tab launch type at creation from tab state. "
                                + "Assuming tab launch type is UNSET");
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
            try {
                long tokenHigh = stream.readLong();
                long tokenLow = stream.readLong();
                Token tabGroupId = new Token(tokenHigh, tokenLow);
                tabState.tabGroupId = tabGroupId.isZero() ? null : tabGroupId;
            } catch (EOFException eof) {
                tabState.tabGroupId = null;
                Log.w(
                        TAG,
                        "Failed to read tabGroupId token from tab state."
                                + " Assuming tabGroupId is null");
            }
            try {
                tabState.tabHasSensitiveContent = stream.readBoolean();
            } catch (EOFException eof) {
                tabState.tabHasSensitiveContent = false;
                Log.w(
                        TAG,
                        "Failed to read tabHasSensitiveContent from tab state. "
                                + "Assuming tabHasSensitiveContent is false");
            }
            // If TabState was restored using legacy format and the FlatBuffer flag is on, that
            // indicates the TabState hasn't been migrated yet and should be.
            if (isMigrateStaleTabsToFlatBufferEnabled()) {
                tabState.shouldMigrate = true;
            }
            return tabState;
        } finally {
            StreamUtil.closeQuietly(stream);
            StreamUtil.closeQuietly(input);
        }
    }

    private static TabState readStateFlatBuffer(
            File file, boolean encrypted, CipherFactory cipherFactory) throws IOException {
        FileInputStream fileInputStream = null;
        CipherInputStream cipherInputStream = null;
        DataInputStream dataInputStream = null;
        try {
            fileInputStream = new FileInputStream(file);
            FlatBufferTabStateSerializer serializer = new FlatBufferTabStateSerializer(encrypted);
            if (encrypted) {
                Cipher cipher = cipherFactory.getCipher(Cipher.DECRYPT_MODE);
                if (cipher == null) {
                    Log.e(
                            TAG,
                            "Cannot restore encrypted TabState FlatBuffer file because cipher is"
                                    + " null");
                    return null;
                }
                cipherInputStream = new CipherInputStream(fileInputStream, cipher);
                dataInputStream = new DataInputStream(cipherInputStream);
                if (dataInputStream.readLong() != KEY_CHECKER) {
                    Log.i(TAG, "Encryption key has changed, cannot restore incognito TabState");
                    return null;
                }
                int size = dataInputStream.readInt();
                byte[] res = new byte[size];
                dataInputStream.readFully(res);
                return serializer.deserialize(ByteBuffer.wrap(res));
            } else {
                FileChannel channel = fileInputStream.getChannel();
                ByteBuffer res = channel.map(MapMode.READ_ONLY, channel.position(), channel.size());
                return serializer.deserialize(res);
            }
        } finally {
            StreamUtil.closeQuietly(dataInputStream);
            StreamUtil.closeQuietly(cipherInputStream);
            StreamUtil.closeQuietly(fileInputStream);
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
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     */
    public static void saveState(
            File directory,
            TabState tabState,
            int tabId,
            boolean isEncrypted,
            CipherFactory cipherFactory) {
        // Save regular hand-written based TabState file when the FlatBuffer flag is both on and
        // off.
        // We must always have a safe fallback to hand-written based TabState to be able to roll out
        // FlatBuffers safely.
        saveStateInternal(
                getTabStateFile(directory, tabId, isEncrypted, false),
                tabState,
                isEncrypted,
                cipherFactory);
    }

    /**
     * Migrate TabState to new FlatBuffer based format
     *
     * @param directory directory TabState files are stored in
     * @param tabState TabState to store in a file
     * @param tabId identifier for the Tab
     * @param isEncrypted whether the stored Tab is encrypted or not
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     * @return true if migration was successful.
     */
    public static boolean migrateTabState(
            File directory,
            TabState tabState,
            int tabId,
            boolean isEncrypted,
            CipherFactory cipherFactory) {
        try {
            saveStateInternal(
                    getTabStateFile(directory, tabId, isEncrypted, true),
                    tabState,
                    isEncrypted,
                    cipherFactory);
            return true;
        } catch (Exception e) {
            // TODO(crbug.com/341122002) Add in metrics
            Log.d(TAG, "Error saving TabState FlatBuffer file", e);
        }
        return false;
    }

    /**
     * @param directory directory TabState files are stored in
     * @param tabId identifier for the {@link Tab}
     * @param isEncrypted true if the {@link Tab} is incognito. Otherwise false.
     * @return true if a {@link Tab} is migrated to the new FlatBuffer format.
     */
    public static boolean isMigrated(File directory, int tabId, boolean isEncrypted) {
        File file = getTabStateFile(directory, tabId, isEncrypted, /* isFlatbuffer= */ true);
        return file != null && file.exists();
    }

    /**
     * Writes the TabState to disk. This method may be called on either the UI or background thread.
     *
     * @param file File to write the tab's state to.
     * @param state State object obtained from from {@link Tab#getState()}.
     * @param encrypted Whether or not the TabState should be encrypted.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void saveStateInternal(
            File file, TabState state, boolean encrypted, CipherFactory cipherFactory) {
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
            if (file.getName().startsWith(FLATBUFFER_PREFIX)) {
                saveStateFlatBuffer(
                        file, state, encrypted, cipherFactory, contentsStateBytes, startTime);
                return;
            }
            fileOutputStream = new FileOutputStream(file);

            if (encrypted) {
                Cipher cipher = cipherFactory.getCipher(Cipher.ENCRYPT_MODE);
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

            dataOutputStream.writeLong(state.timestampMillis);
            dataOutputStream.writeInt(contentsStateBytes.length);
            dataOutputStream.write(contentsStateBytes);
            dataOutputStream.writeInt(state.parentId);
            dataOutputStream.writeUTF(state.openerAppId != null ? state.openerAppId : "");
            dataOutputStream.writeInt(state.contentsState.version());
            dataOutputStream.writeLong(-1); // Obsolete sync ID.
            dataOutputStream.writeBoolean(false); // Obsolete attribute |SHOULD_PRESERVE|.
            dataOutputStream.writeInt(state.themeColor);
            dataOutputStream.writeInt(state.tabLaunchTypeAtCreation);
            dataOutputStream.writeInt(state.rootId);
            dataOutputStream.writeInt(state.userAgent);
            dataOutputStream.writeLong(state.lastNavigationCommittedTimestampMillis);
            long tokenHigh = NO_TAB_GROUP_ID;
            long tokenLow = NO_TAB_GROUP_ID;
            if (state.tabGroupId != null) {
                tokenHigh = state.tabGroupId.getHigh();
                tokenLow = state.tabGroupId.getLow();
            }
            dataOutputStream.writeLong(tokenHigh);
            dataOutputStream.writeLong(tokenLow);
            dataOutputStream.writeBoolean(state.tabHasSensitiveContent);
            long saveTime = SystemClock.elapsedRealtime() - startTime;
            RecordHistogram.recordTimesHistogram("Tabs.TabState.SaveTime", saveTime);
            RecordHistogram.recordTimesHistogram("Tabs.TabState.SaveTime.Legacy", saveTime);
        } catch (FileNotFoundException e) {
            Log.w(TAG, "FileNotFoundException while attempting to save TabState.");
        } catch (IOException e) {
            Log.w(TAG, "IOException while attempting to save TabState.");
        } finally {
            StreamUtil.closeQuietly(dataOutputStream);
            StreamUtil.closeQuietly(fileOutputStream);
        }
    }

    private static void saveStateFlatBuffer(
            File file,
            TabState state,
            boolean encrypted,
            CipherFactory cipherFactory,
            byte[] contentsStateBytes,
            long startTime) {
        FileOutputStream fileOutputStream = null;
        CipherOutputStream cipherOutputStream = null;
        DataOutputStream dataOutputStream = null;
        boolean success = false;
        AtomicFile atomicFile = new AtomicFile(file);
        try {
            fileOutputStream = atomicFile.startWrite();
            FlatBufferTabStateSerializer serializer = new FlatBufferTabStateSerializer(encrypted);
            ByteBuffer data = serializer.serialize(state, contentsStateBytes);
            if (encrypted) {
                Cipher cipher = cipherFactory.getCipher(Cipher.ENCRYPT_MODE);
                if (cipher == null) {
                    Log.e(TAG, "Cannot save TabState FlatBuffer file because cipher is null");
                    return;
                }
                cipherOutputStream = new CipherOutputStream(fileOutputStream, cipher);
                dataOutputStream = new DataOutputStream(cipherOutputStream);
                dataOutputStream.writeLong(KEY_CHECKER);
                int size = data.remaining();
                dataOutputStream.writeInt(size);
                WritableByteChannel channel = Channels.newChannel(dataOutputStream);
                channel.write(data);
            } else {
                FileChannel channel = fileOutputStream.getChannel();
                channel.write(data);
            }
            success = true;
            RecordHistogram.recordTimesHistogram(
                    "Tabs.TabState.SaveTime.FlatBuffer", SystemClock.elapsedRealtime() - startTime);
        } catch (Throwable e) {
            // Catch all in case of an issue saving the FlatBuffer file. Avoid crashing
            // the app and simply log what went wrong.
            Log.e(TAG, "Exception writing " + file.getName(), e);
        } finally {
            StreamUtil.closeQuietly(dataOutputStream);
            StreamUtil.closeQuietly(cipherOutputStream);
            StreamUtil.closeQuietly(fileOutputStream);
            if (success) {
                safelyFinishOrFailWrite(atomicFile, fileOutputStream);
            } else {
                safelyFailWrite(atomicFile, fileOutputStream);
            }
        }
    }

    private static void safelyFinishOrFailWrite(
            AtomicFile atomicFile, FileOutputStream fileOutputStream) {
        try {
            atomicFile.finishWrite(fileOutputStream);
        } catch (Throwable e) {
            Log.e(TAG, "Error finishing atomic write of " + atomicFile, e);
            safelyFailWrite(atomicFile, fileOutputStream);
        }
    }

    private static void safelyFailWrite(AtomicFile atomicFile, FileOutputStream fileOutputStream) {
        try {
            atomicFile.failWrite(fileOutputStream);
        } catch (Throwable e) {
            Log.e(TAG, "Error failing atomic write of " + atomicFile, e);
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
     * Delete migrated TabState file for corresponding Tab
     *
     * @param directory directory TabState files are stored in
     * @param tabId identifier for {@link Tab}
     * @param encrypted isEncrypted true if the {@link Tab} is incognito. Otherwise false.
     */
    public static void deleteMigratedFile(File directory, int tabId, boolean encrypted) {
        File file = getTabStateFile(directory, tabId, encrypted, /* isFlatbuffer= */ true);
        if (file != null && file.exists() && !file.delete()) {
            Log.e(TAG, "Failed to delete TabState: " + file);
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

    @VisibleForTesting
    protected static void deleteFlatBufferFiles(File stateDirectory) {
        if (stateDirectory == null || stateDirectory.listFiles() == null) {
            return;
        }
        for (String filename :
                stateDirectory.list(
                        new FilenameFilter() {
                            @Override
                            public boolean accept(File dir, String name) {
                                return name != null && name.startsWith(FLATBUFFER_PREFIX);
                            }
                        })) {
            File file = new File(stateDirectory, filename);
            if (!file.delete()) {
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

    private static boolean isMigrateStaleTabsToFlatBufferEnabled() {
        return MIGRATE_STALE_TABS_CACHED_PARAM.getValue();
    }
}
