// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.ArchivePersistedTabData.ArchivePersistedTabDataProto;

import java.nio.ByteBuffer;

/** {@link PersistedTabData} for archiving/auto-deleting inactive tabs. */
public class ArchivePersistedTabData extends PersistedTabData {
    @VisibleForTesting protected static final long INVALID_TIMESTAMP = -1;
    private static final String TAG = "ArchivePTD";
    private static final Class<ArchivePersistedTabData> USER_DATA_KEY =
            ArchivePersistedTabData.class;

    private long mArchivedTimeMs = INVALID_TIMESTAMP;

    /**
     * Gets the {@link ArchivePersistedTabData} associated with the given tab or creates one if it
     * doesn't exist. If the tab data is created, then the current timestamp is used as the archive
     * time.
     *
     * @param tab The {@link Tab} to get/create the tab data for.
     * @param callback The {@link Callback} to be invoked when the {@link ArchivePersistedTabData}
     *     is ready.
     */
    public static void from(Tab tab, Callback<ArchivePersistedTabData> callback) {
        PersistedTabData.from(tab, () -> new ArchivePersistedTabData(tab), USER_DATA_KEY, callback);
    }

    @VisibleForTesting
    protected static ArchivePersistedTabData from(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new ArchivePersistedTabData(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private ArchivePersistedTabData(Tab tab) {
        super(
                tab,
                PersistedTabDataConfiguration.get(ArchivePersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(ArchivePersistedTabData.class, tab.isIncognito())
                        .getId());
    }

    public long getArchivedTimeMs() {
        return mArchivedTimeMs;
    }

    public void setArchivedTimeMs(long archivedTimeMs) {
        mArchivedTimeMs = archivedTimeMs;
        save();
    }

    // PersistedTabData implementation.

    @Override
    Serializer<ByteBuffer> getSerializer() {
        return () ->
                ArchivePersistedTabDataProto.newBuilder()
                        .setArchivedTimeMs(mArchivedTimeMs)
                        .build()
                        .toByteString()
                        .asReadOnlyByteBuffer();
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
        if (bytes == null || !bytes.hasRemaining()) return false;

        try {
            mArchivedTimeMs = ArchivePersistedTabDataProto.parseFrom(bytes).getArchivedTimeMs();
        } catch (InvalidProtocolBufferException e) {
            Log.i(TAG, "deserialize failed: \n" + e.toString());
            return false;
        }

        return true;
    }

    @Override
    public String getUmaTag() {
        return TAG;
    }
}
