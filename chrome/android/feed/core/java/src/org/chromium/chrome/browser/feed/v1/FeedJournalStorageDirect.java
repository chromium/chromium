// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.common.Result;

import java.util.List;

/**
 * Wrapper around {@link JournalStorage}, providing a synchronous implementation.
 */
public final class FeedJournalStorageDirect implements JournalStorageDirect {
    private static final String LOCATION = "FeedJournalStorageDirect.";
    private final JournalStorage mJournalStorage;

    FeedJournalStorageDirect(JournalStorage journalStorage) {
        this.mJournalStorage = journalStorage;
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        return FutureTaskConsumer.consume(LOCATION + "read",
                (consumer) -> mJournalStorage.read(journalName, consumer), Result.failure());
    }

    @Override
    public CommitResult commit(JournalMutation mutation) {
        return FutureTaskConsumer.consume(LOCATION + "commit",
                (consumer) -> mJournalStorage.commit(mutation, consumer), CommitResult.FAILURE);
    }

    @Override
    public Result<Boolean> exists(String journalName) {
        return FutureTaskConsumer.consume(LOCATION + "exists",
                (consumer) -> mJournalStorage.exists(journalName, consumer), Result.failure());
    }

    @Override
    public Result<List<String>> getAllJournals() {
        return FutureTaskConsumer.consume(
                LOCATION + "getAllJournals", mJournalStorage::getAllJournals, Result.failure());
    }

    @Override
    public CommitResult deleteAll() {
        return FutureTaskConsumer.consume(
                LOCATION + "deleteAll", mJournalStorage::deleteAll, CommitResult.FAILURE);
    }
}
