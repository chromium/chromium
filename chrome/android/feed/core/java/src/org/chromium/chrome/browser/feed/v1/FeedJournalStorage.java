// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.List;

/**
 * Implementation of {@link JournalStorage} that persisits data on native side.
 */
public class FeedJournalStorage implements JournalStorage {
    private FeedJournalBridge mFeedJournalBridge;

    /**
     * Creates a {@link FeedJournalStorage} for storing journals for the current user.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedJournalStorage(Profile profile) {
        mFeedJournalBridge = new FeedJournalBridge(profile);
    }

    /**
     * Creates a {@link FeedJournalStorage} for testing.
     *
     * @param bridge {@link FeedJournalBridge} implementation can handle journal storage request.
     */
    @VisibleForTesting
    public FeedJournalStorage(FeedJournalBridge bridge) {
        mFeedJournalBridge = bridge;
    }

    /** Cleans up {@link FeedJournalStorage}. */
    public void destroy() {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.destroy();
        mFeedJournalBridge = null;
    }

    @Override
    public void read(String journalName, Consumer<Result<List<byte[]>>> consumer) {
        if (mFeedJournalBridge == null) {
            consumer.accept(Result.failure());
        } else {
            mFeedJournalBridge.loadJournal(journalName, (byte[][] entries) -> {
                List<byte[]> journal = Arrays.asList(entries);
                consumer.accept(Result.success(journal));
            }, (Void ignored) -> consumer.accept(Result.failure()));
        }
    }

    @Override
    public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
        if (mFeedJournalBridge == null) {
            consumer.accept(CommitResult.FAILURE);
        } else {
            mFeedJournalBridge.commitJournalMutation(mutation,
                    (Boolean result)
                            -> consumer.accept(
                                    result ? CommitResult.SUCCESS : CommitResult.FAILURE));
        }
    }

    @Override
    public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
        if (mFeedJournalBridge == null) {
            consumer.accept(Result.failure());
        } else {
            mFeedJournalBridge.doesJournalExist(journalName,
                    (Boolean exist)
                            -> consumer.accept(Result.success(exist)),
                    (Void ignored) -> consumer.accept(Result.failure()));
        }
    }

    @Override
    public void getAllJournals(Consumer<Result<List<String>>> consumer) {
        if (mFeedJournalBridge == null) {
            consumer.accept(Result.failure());
        } else {
            mFeedJournalBridge.loadAllJournalKeys(
                    (String[] data)
                            -> consumer.accept(Result.success(Arrays.asList(data))),
                    (Void ignored) -> consumer.accept(Result.failure()));
        }
    }

    @Override
    public void deleteAll(Consumer<CommitResult> consumer) {
        if (mFeedJournalBridge == null) {
            consumer.accept(CommitResult.FAILURE);
        } else {
            mFeedJournalBridge.deleteAllJournals(
                    (Boolean result)
                            -> consumer.accept(
                                    result ? CommitResult.SUCCESS : CommitResult.FAILURE));
        }
    }
}
