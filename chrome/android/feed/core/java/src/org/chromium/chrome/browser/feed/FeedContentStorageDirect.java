// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.common.Result;

import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Wrapper around {@link ContentStorage}, providing a synchronous implementation.
 */
public final class FeedContentStorageDirect implements ContentStorageDirect {
    private static final String LOCATION = "FeedContentStorageDirect.";
    private final ContentStorage mContentStorage;

    FeedContentStorageDirect(ContentStorage contentStorage) {
        mContentStorage = contentStorage;
    }

    @Override
    public Result<Map<String, byte[]>> get(List<String> keys) {
        if (keys.isEmpty()) {
            return Result.success(Collections.emptyMap());
        }

        return FutureTaskConsumer.consume(LOCATION + "get",
                (consumer) -> mContentStorage.get(keys, consumer), Result.failure());
    }

    @Override
    public Result<Map<String, byte[]>> getAll(String prefix) {
        return FutureTaskConsumer.consume(LOCATION + "getAll",
                (consumer) -> mContentStorage.getAll(prefix, consumer), Result.failure());
    }

    @Override
    public CommitResult commit(ContentMutation mutation) {
        return FutureTaskConsumer.consume(LOCATION + "commit",
                (consumer) -> mContentStorage.commit(mutation, consumer), CommitResult.FAILURE);
    }

    @Override
    public Result<List<String>> getAllKeys() {
        return FutureTaskConsumer.consume(
                LOCATION + "getAllKeys", mContentStorage::getAllKeys, Result.failure());
    }
}
