// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.storage.CommitResult;
import com.google.android.libraries.feed.host.storage.JournalMutation;
import com.google.android.libraries.feed.host.storage.JournalStorage;

import org.chromium.base.VisibleForTesting;
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
    public void read(String journalName, Consumer < Result < List<byte[]>>> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.loadJournal(journalName, (byte[][] entries) -> {
            List<byte[]> journal = Arrays.asList(entries);
            consumer.accept(Result.success(journal));
        }, (Void ignored) -> consumer.accept(Result.failure()));
    }

    @Override
    public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.commitJournalMutation(mutation,
                (Boolean result)
                        -> consumer.accept(result ? CommitResult.SUCCESS : CommitResult.FAILURE));
    }

    @Override
    public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.doesJournalExist(journalName,
                (Boolean exist)
                        -> consumer.accept(Result.success(exist)),
                (Void ignored) -> consumer.accept(Result.failure()));
    }

    @Override
    public void getAllJournals(Consumer < Result < List<String>>> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.loadAllJournalKeys(
                (String[] data)
                        -> consumer.accept(Result.success(Arrays.asList(data))),
                (Void ignored) -> consumer.accept(Result.failure()));
    }

    @Override
    public void deleteAll(Consumer<CommitResult> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.deleteAllJournals(
                (Boolean result)
                        -> consumer.accept(result ? CommitResult.SUCCESS : CommitResult.FAILURE));
    }
}
