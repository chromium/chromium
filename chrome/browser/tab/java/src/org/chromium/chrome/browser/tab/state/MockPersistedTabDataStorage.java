// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Semaphore;

/**
 * Mock implementation of {@link PersistedTabDataStorage} for tests
 */
public class MockPersistedTabDataStorage implements PersistedTabDataStorage {
    private Semaphore mSemaphore;
    private final Map<String, byte[]> mStorage = new HashMap<>();

    @Override
    public void save(int tabId, String tabDataId, Supplier<byte[]> dataSupplier) {
        mStorage.put(getKey(tabId), dataSupplier.get());
        if (mSemaphore != null) {
            mSemaphore.release();
        }
    }

    @Override
    public void restore(int tabId, String tabDataId, Callback<byte[]> callback) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { callback.onResult(mStorage.get(getKey(tabId))); });
        if (mSemaphore != null) {
            mSemaphore.release();
        }
    }

    // Unused
    @Override
    public byte[] restore(int tabId, String tabDataId) {
        return null;
    }

    @Override
    public void delete(int tabId, String tabDataId) {
        mStorage.remove(getKey(tabId));
        if (mSemaphore != null) {
            mSemaphore.release();
        }
    }

    @Override
    public String getUmaTag() {
        return "MPTDS";
    }

    private static String getKey(int tabId) {
        return String.format(Locale.US, "%d", tabId);
    }

    public void setSemaphore(Semaphore semaphore) {
        mSemaphore = semaphore;
    }
}
