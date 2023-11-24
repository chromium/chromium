// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * Mock implementation of {@link PersistedTabDataStorage} for tests. Specifically
 * this implementation mocks a non-null Bytebuffer with limit 0 simulating what
 * we saw in crbug.com/1287632.
 */
public class EmptyByteBufferPersistedTabDataStorage implements PersistedTabDataStorage {
    // Unused
    @Override
    public void save(int tabId, String tabDataId, Serializer<ByteBuffer> serializer) {
        assert false : "save is currently unused in EmptyByteBufferPersistedTabDataStorage";
    }

    @Override
    public void save(
            int tabId,
            String tabDataId,
            Serializer<ByteBuffer> serializer,
            Callback<Integer> callback) {
        assert false : "save is currently unused in EmptyByteBufferPersistedTabDataStorage";
    }

    @Override
    public void restore(int tabId, String tabDataId, Callback<ByteBuffer> callback) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(ByteBuffer.allocateDirect(0));
                });
    }

    // Unused
    @Override
    public ByteBuffer restore(int tabId, String tabDataId) {
        return ByteBuffer.allocateDirect(0);
    }

    @Override
    public <U extends PersistedTabDataResult> U restore(
            int tabId, String dataId, PersistedTabDataMapper<U> mapper) {
        return mapper.map(ByteBuffer.allocateDirect(0));
    }

    @Override
    public <U extends PersistedTabDataResult> void restore(
            int tabId, String dataId, Callback<U> callback, PersistedTabDataMapper<U> mapper) {
        callback.onResult(mapper.map(ByteBuffer.allocateDirect(0)));
    }

    // Unused
    @Override
    public void delete(int tabId, String tabDataId) {
        assert false : "delete is currently unused in EmptyByteBufferPersistedTabDataStorage";
    }

    @Override
    public String getUmaTag() {
        return "MPTDS";
    }

    @Override
    public void performMaintenance(List<Integer> tabIds, String dataId) {
        assert false : "perforMaintenance is not available in MockPersistedTabDataStorage";
    }
}
