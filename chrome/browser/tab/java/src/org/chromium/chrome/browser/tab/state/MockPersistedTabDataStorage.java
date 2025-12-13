// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Semaphore;

/** Mock implementation of {@link PersistedTabDataStorage} for tests */
@NullMarked
public class MockPersistedTabDataStorage implements PersistedTabDataStorage {
    private @Nullable Semaphore mSemaphore;
    private final Map<String, ByteBuffer> mStorage = new HashMap<>();

    @Override
    public void save(int tabId, String tabDataId, Serializer<ByteBuffer> serializer) {
        serializer.preSerialize();
        mStorage.put(getKey(tabId), serializer.get());
        if (mSemaphore != null) {
            mSemaphore.release();
        }
    }

    @Override
    public void save(
            int tabId,
            String dataId,
            Serializer<ByteBuffer> serializer,
            Callback<Integer> callback) {
        save(tabId, dataId, serializer);
        callback.onResult(0);
    }

    @Override
    public void restore(int tabId, String tabDataId, Callback<@Nullable ByteBuffer> callback) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(
                            mStorage.get(getKey(tabId)) == null
                                    ? null
                                    : mStorage.get(getKey(tabId)));
                });
        if (mSemaphore != null) {
            mSemaphore.release();
        }
    }

    // Unused
    @Override
    public @Nullable ByteBuffer restore(int tabId, String tabDataId) {
        return null;
    }

    @Override
    public <U extends PersistedTabDataResult> @Nullable U restore(
            int tabId, String dataId, PersistedTabDataMapper<U> mapper) {
        assert false : "Restore with mapper currently unused in MockPersistedTabDataStorage";
        return null;
    }

    @Override
    public <U extends PersistedTabDataResult> void restore(
            int tabId, String dataId, Callback<U> callback, PersistedTabDataMapper<U> mapper) {
        assert false : "Restore with mapper currently unused in MockPersistedTabDataStorage";
    }

    @Override
    public void delete(int tabId, String tabDataId) {
        mStorage.remove(getKey(tabId));
        if (mSemaphore != null) {
            mSemaphore.release();
        }
    }

    @Override
    public void performMaintenance(List<Integer> tabIds, String dataId) {
        assert false : "performMaintenance is not available in MockPersistedTabDataStorage";
    }

    private static String getKey(int tabId) {
        return String.format(Locale.US, "%d", tabId);
    }

    public void setSemaphore(Semaphore semaphore) {
        mSemaphore = semaphore;
    }
}
