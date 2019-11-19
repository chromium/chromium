// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;

/**
 * Object that contains the state of a tab, including its navigation history.
 */
public class TabState {
    private static final String TAG = "TabState";

    public static final String SAVED_TAB_STATE_FILE_PREFIX = "tab";
    public static final String SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO = "cryptonito";

    /**
     * Version number of the format used to save the WebContents navigation history, as returned by
     * TabStateJni.get().getContentsStateAsByteBuffer(). Version labels:
     *   0 - Chrome m18
     *   1 - Chrome m25
     *   2 - Chrome m26+
     */
    public static final int CONTENTS_STATE_CURRENT_VERSION = 2;

    /** Special value for mTimestampMillis. */
    private static final long TIMESTAMP_NOT_SET = -1;

    /** Checks if the TabState header is loaded properly. */
    private static final long KEY_CHECKER = 0;

    /**
     * There's no official maximum size for a bundle, but if a Binder transaction fails and the
     * parcel was bigger than 200kB, the platform blames it on the bundle being too large.
     */
    private static final int MAX_BUNDLE_SIZE = 200 * 1024;

    /** A theme color that indicates an unspecified state. */
    public static final int UNSPECIFIED_THEME_COLOR = Color.TRANSPARENT;

    private static final String TAB_STATE_BUNDLE_PREFIX = "tab_";
    private static final String TIMESTAMP_MILLIS = TAB_STATE_BUNDLE_PREFIX + "timestampMillis";
    private static final String CONTENT_STATE_BYTES =
            TAB_STATE_BUNDLE_PREFIX + "contentsStateBytes";
    private static final String PARENT_ID = TAB_STATE_BUNDLE_PREFIX + "parentId";
    private static final String OPENER_APP_ID = TAB_STATE_BUNDLE_PREFIX + "openerAppId";
    private static final String VERSION = TAB_STATE_BUNDLE_PREFIX + "version";
    private static final String THEME_COLOR = TAB_STATE_BUNDLE_PREFIX + "themeColor";
    private static final String IS_INCOGNITO = TAB_STATE_BUNDLE_PREFIX + "isIncognito";

    /** Overrides the Chrome channel/package name to test a variant channel-specific behaviour. */
    private static String sChannelNameOverrideForTest;

    /** Contains the state for a WebContents. */
    public static class WebContentsState {
        private final ByteBuffer mBuffer;
        private int mVersion;

        public WebContentsState(ByteBuffer buffer) {
            mBuffer = buffer;
        }

        public ByteBuffer buffer() {
            return mBuffer;
        }

        public int version() {
            return mVersion;
        }

        public void setVersion(int version) {
            mVersion = version;
        }

        /**
         * Creates a WebContents from the buffer.
         * @param isHidden Whether or not the tab initially starts hidden.
         * @return Pointer A WebContents object.
         */
        public WebContents restoreContentsFromByteBuffer(boolean isHidden) {
            return TabStateJni.get().restoreContentsFromByteBuffer(mBuffer, mVersion, isHidden);
        }

        /**
         * Deletes navigation entries from this buffer matching predicate.
         * @param predicate Handle for a deletion predicate interpreted by native code.
                            Only valid during this call frame.
         * @return WebContentsState A new state or null if nothing changed.
         */
        @Nullable
        public WebContentsState deleteNavigationEntries(long predicate) {
            ByteBuffer newBuffer =
                    TabStateJni.get().deleteNavigationEntries(mBuffer, mVersion, predicate);
            if (newBuffer == null) return null;
            WebContentsState newState = new TabState.WebContentsState(newBuffer);
            newState.setVersion(TabState.CONTENTS_STATE_CURRENT_VERSION);
            return newState;
        }
    }

    /** Navigation history of the WebContents. */
    public WebContentsState contentsState;
    public int parentId = Tab.INVALID_TAB_ID;
    public int rootId;

    public long timestampMillis = TIMESTAMP_NOT_SET;
    public String openerAppId;

    /**
     * The tab's brand theme color. Set this to {@link #UNSPECIFIED_THEME_COLOR} for an unspecified
     * state.
     */
    public int themeColor = UNSPECIFIED_THEME_COLOR;

    public @Nullable @TabLaunchType Integer tabLaunchTypeAtCreation;

    /** Whether this TabState was created from a file containing info about an incognito Tab. */
    protected boolean mIsIncognito;

    /** @return Whether a Stable channel build of Chrome is being used. */
    private static boolean isStableChannelBuild() {
        if ("stable".equals(sChannelNameOverrideForTest)) return true;
        return ChromeVersionInfo.isStableBuild();
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
        return restoreTabState(file, encrypted);
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
            tabState = TabState.readState(stream, isEncrypted);
        } catch (FileNotFoundException exception) {
            Log.e(TAG, "Failed to restore tab state for tab: " + tabFile);
        } catch (IOException exception) {
            Log.e(TAG, "Failed to restore tab state.", exception);
        } finally {
            StreamUtil.closeQuietly(stream);
        }
        return tabState;
    }

    /**
     * Restores a particular TabState file from the provided Bundle.
     * @param bundle The Bundle to restore TabState from.
     * @return TabState that has been restored.
     */
    public static TabState restoreTabState(Bundle bundle) {
        TabState tabState = new TabState();
        tabState.timestampMillis = bundle.getLong(TIMESTAMP_MILLIS);
        byte[] bytes = bundle.getByteArray(CONTENT_STATE_BYTES);
        tabState.contentsState = new WebContentsState(ByteBuffer.allocateDirect(bytes.length));
        tabState.contentsState.buffer().put(bytes);
        tabState.parentId = bundle.getInt(PARENT_ID);
        tabState.openerAppId = bundle.getString(OPENER_APP_ID);
        tabState.contentsState.setVersion(bundle.getInt(VERSION));
        tabState.themeColor = bundle.getInt(THEME_COLOR);
        tabState.mIsIncognito = bundle.getBoolean(IS_INCOGNITO);

        return tabState;
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
            tabState.mIsIncognito = encrypted;
            try {
                tabState.themeColor = stream.readInt();
            } catch (EOFException eof) {
                // Could happen if reading a version of TabState without a theme color.
                tabState.themeColor = UNSPECIFIED_THEME_COLOR;
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
            return tabState;
        } finally {
            stream.close();
        }
    }

    private static byte[] getContentStateByteArray(ByteBuffer buffer) {
        byte[] contentsStateBytes = new byte[buffer.limit()];
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            buffer.rewind();
            buffer.get(contentsStateBytes);
        } else {
            // For JellyBean and below a bug in MappedByteBufferAdapter causes rewind to not be
            // propagated to the underlying ByteBuffer, and results in an underflow exception. See:
            // http://b.android.com/53637.
            for (int i = 0; i < buffer.limit(); i++) contentsStateBytes[i] = buffer.get(i);
        }
        return contentsStateBytes;
    }

    /** @return An opaque "state" object that can be persisted to storage. */
    public static TabState from(Tab tab) {
        if (!tab.isInitialized()) return null;
        TabState tabState = new TabState();
        tabState.contentsState = getWebContentsState(tab);
        tabState.openerAppId = TabAssociatedApp.getAppId(tab);
        tabState.parentId = tab.getParentId();
        tabState.timestampMillis = tab.getTimestampMillis();
        tabState.tabLaunchTypeAtCreation = tab.getLaunchTypeAtInitialTabCreation();
        // Don't save the actual default theme color because it could change on night mode state
        // changed.
        tabState.themeColor = TabThemeColorHelper.isUsingColorFromTabContents(tab)
                ? TabThemeColorHelper.getColor(tab)
                : TabState.UNSPECIFIED_THEME_COLOR;
        tabState.rootId = tab.getRootId();
        return tabState;
    }

    /** Returns an object representing the state of the Tab's WebContents. */
    private static WebContentsState getWebContentsState(Tab tab) {
        if (tab.getFrozenContentsState() != null) return tab.getFrozenContentsState();

        // Native call returns null when buffer allocation needed to serialize the state failed.
        ByteBuffer buffer = getWebContentsStateAsByteBuffer(tab);
        if (buffer == null) return null;

        WebContentsState state = new WebContentsState(buffer);
        state.setVersion(CONTENTS_STATE_CURRENT_VERSION);
        return state;
    }

    /** Returns an ByteBuffer representing the state of the Tab's WebContents. */
    private static ByteBuffer getWebContentsStateAsByteBuffer(Tab tab) {
        LoadUrlParams pendingLoadParams = tab.getPendingLoadParams();
        if (pendingLoadParams == null) {
            return getContentsStateAsByteBuffer(tab);
        } else {
            Referrer referrer = pendingLoadParams.getReferrer();
            return createSingleNavigationStateAsByteBuffer(pendingLoadParams.getUrl(),
                    referrer != null ? referrer.getUrl() : null,
                    // Policy will be ignored for null referrer url, 0 is just a placeholder.
                    referrer != null ? referrer.getPolicy() : 0,
                    pendingLoadParams.getInitiatorOrigin(), tab.isIncognito());
        }
    }

    /**
     * Writes the TabState to disk. This method may be called on either the UI or background thread.
     * @param file File to write the tab's state to.
     * @param state State object obtained from from {@link Tab#getState()}.
     * @param encrypted Whether or not the TabState should be encrypted.
     */
    public static void saveState(File file, TabState state, boolean encrypted) {
        if (state == null || state.contentsState == null) return;

        // Create the byte array from contentsState before opening the FileOutputStream, in case
        // contentsState.buffer is an instance of MappedByteBuffer that is mapped to
        // the tab state file.
        byte[] contentsStateBytes = getContentStateByteArray(state.contentsState.buffer());

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
     * Writes the TabState to a bundle. This method may be called on either the UI or background
     * thread.
     * @param bundle Bundle to write the tab's state to.
     * @param state State object obtained from from {@link Tab#getState()}.
     * @return Whether the tab state was successfully saved.
     */
    public static boolean saveState(Bundle bundle, TabState state) {
        if (state == null || state.contentsState == null) return false;

        byte[] contentsStateBytes = getContentStateByteArray(state.contentsState.buffer());

        // Chrome Tab State only goes back 50 navigations, and the size of TabState typically caps
        // out around 50kB.
        // TODO(mthiesse): If this starts getting hit we'll need to reduce the history size or fall
        // back to saving to disk.
        assert contentsStateBytes.length < MAX_BUNDLE_SIZE;

        bundle.putLong(TIMESTAMP_MILLIS, state.timestampMillis);
        bundle.putByteArray(CONTENT_STATE_BYTES, contentsStateBytes);
        bundle.putInt(PARENT_ID, state.parentId);
        bundle.putString(OPENER_APP_ID, state.openerAppId);
        bundle.putInt(VERSION, state.contentsState.version());
        bundle.putInt(THEME_COLOR, state.themeColor);
        bundle.putBoolean(IS_INCOGNITO, state.isIncognito());
        return true;
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

    /** @return Title currently being displayed in the saved state's current entry. */
    public String getDisplayTitleFromState() {
        return TabStateJni.get().getDisplayTitleFromByteBuffer(
                contentsState.buffer(), contentsState.version());
    }

    /** @return URL currently being displayed in the saved state's current entry. */
    public String getVirtualUrlFromState() {
        return TabStateJni.get().getVirtualUrlFromByteBuffer(
                contentsState.buffer(), contentsState.version());
    }

    /** @return Whether an incognito TabState was loaded by {@link #readState}. */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /** @return The theme color of the tab or {@link #UNSPECIFIED_THEME_COLOR} if not set. */
    public int getThemeColor() {
        return themeColor;
    }

    /** @return True if the tab has a theme color set. */
    public boolean hasThemeColor() {
        return themeColor != UNSPECIFIED_THEME_COLOR && ColorUtils.isValidThemeColor(themeColor);
    }

    /**
     * Creates a WebContentsState for a tab that will be loaded lazily.
     * @param url URL that is pending.
     * @param referrerUrl URL for the referrer.
     * @param referrerPolicy Policy for the referrer.
     * @param initiatorOrigin Initiator of the navigation.
     * @param isIncognito Whether or not the state is meant to be incognito (e.g. encrypted).
     * @return ByteBuffer that represents a state representing a single pending URL.
     */
    public static ByteBuffer createSingleNavigationStateAsByteBuffer(String url, String referrerUrl,
            int referrerPolicy, String initiatorOrigin, boolean isIncognito) {
        return TabStateJni.get().createSingleNavigationStateAsByteBuffer(
                url, referrerUrl, referrerPolicy, initiatorOrigin, isIncognito);
    }

    /**
     * Returns the WebContents' state as a ByteBuffer.
     * @param tab Tab to pickle.
     * @return ByteBuffer containing the state of the WebContents.
     */
    public static ByteBuffer getContentsStateAsByteBuffer(Tab tab) {
        return TabStateJni.get().getContentsStateAsByteBuffer(tab);
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

    /**
     * Overrides the channel name for testing.
     * @param name Channel to use.
     */
    @VisibleForTesting
    public static void setChannelNameOverrideForTest(String name) {
        sChannelNameOverrideForTest = name;
    }

    /**
     * Creates a historical tab from a tab being closed.
     */
    public static void createHistoricalTab(Tab tab) {
        if (!tab.isFrozen()) {
            TabStateJni.get().createHistoricalTabFromContents(tab.getWebContents());
        } else {
            WebContentsState state = tab.getFrozenContentsState();
            if (state != null) {
                TabStateJni.get().createHistoricalTab(state.buffer(), state.version());
            }
        }
    }

    @NativeMethods
    interface Natives {
        WebContents restoreContentsFromByteBuffer(
                ByteBuffer buffer, int savedStateVersion, boolean initiallyHidden);
        ByteBuffer getContentsStateAsByteBuffer(Tab tab);
        ByteBuffer deleteNavigationEntries(ByteBuffer state, int saveStateVersion, long predicate);
        ByteBuffer createSingleNavigationStateAsByteBuffer(String url, String referrerUrl,
                int referrerPolicy, String initiatorOrigin, boolean isIncognito);
        String getDisplayTitleFromByteBuffer(ByteBuffer state, int savedStateVersion);
        String getVirtualUrlFromByteBuffer(ByteBuffer state, int savedStateVersion);
        void createHistoricalTab(ByteBuffer state, int savedStateVersion);
        void createHistoricalTabFromContents(WebContents webContents);
    }
}
