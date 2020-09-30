// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Append;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Copy;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to native implementations of journal storage.
 */
@JNINamespace("feed")
public class FeedJournalBridge {
    private long mNativeFeedJournalBridge;

    /**
     * Creates a {@link FeedJournalBridge} for accessing native journal storage
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */

    public FeedJournalBridge(Profile profile) {
        mNativeFeedJournalBridge = FeedJournalBridgeJni.get().init(FeedJournalBridge.this, profile);
    }

    /**
     * Creates a {@link FeedJournalBridge} for testing.
     */
    @VisibleForTesting
    public FeedJournalBridge() {}

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedJournalBridge != 0;
        FeedJournalBridgeJni.get().destroy(mNativeFeedJournalBridge, FeedJournalBridge.this);
        mNativeFeedJournalBridge = 0;
    }

    /** Loads the journal and asynchronously returns the contents. */
    public void loadJournal(String journalName, Callback<byte[][]> successCallback,
            Callback<Void> failureCallback) {
        assert mNativeFeedJournalBridge != 0;
        FeedJournalBridgeJni.get().loadJournal(mNativeFeedJournalBridge, FeedJournalBridge.this,
                journalName, successCallback, failureCallback);
    }

    /**
     * Commits the operations in {@link JournalMutation} in order and asynchronously reports the
     * {@link CommitResult}. If all the operations succeed, {@code callback} is called with a
     * success result. If any operation fails, {@code callback} is called with a failure result and
     * the remaining operations are not processed.
     */
    public void commitJournalMutation(JournalMutation mutation, Callback<Boolean> callback) {
        assert mNativeFeedJournalBridge != 0;

        FeedJournalBridgeJni.get().startJournalMutation(
                mNativeFeedJournalBridge, FeedJournalBridge.this, mutation.getJournalName());
        for (JournalOperation operation : mutation.getOperations()) {
            switch (operation.getType()) {
                case Type.APPEND:
                    Append append = (Append) operation;
                    FeedJournalBridgeJni.get().addAppendOperation(
                            mNativeFeedJournalBridge, FeedJournalBridge.this, append.getValue());
                    break;
                case Type.COPY:
                    Copy copy = (Copy) operation;
                    FeedJournalBridgeJni.get().addCopyOperation(mNativeFeedJournalBridge,
                            FeedJournalBridge.this, copy.getToJournalName());
                    break;
                case Type.DELETE:
                    FeedJournalBridgeJni.get().addDeleteOperation(
                            mNativeFeedJournalBridge, FeedJournalBridge.this);
                    break;
                default:
                    // Operation type is not supported, therefore failing immediately.
                    assert false : "Unsupported type of operation.";
                    FeedJournalBridgeJni.get().deleteJournalMutation(
                            mNativeFeedJournalBridge, FeedJournalBridge.this);
                    callback.onResult(false);
                    return;
            }
        }
        FeedJournalBridgeJni.get().commitJournalMutation(
                mNativeFeedJournalBridge, FeedJournalBridge.this, callback);
    }

    /** Determines whether a journal exists and asynchronously responds. */
    public void doesJournalExist(
            String journalName, Callback<Boolean> successCallback, Callback<Void> failureCallback) {
        assert mNativeFeedJournalBridge != 0;
        FeedJournalBridgeJni.get().doesJournalExist(mNativeFeedJournalBridge,
                FeedJournalBridge.this, journalName, successCallback, failureCallback);
    }

    /** Asynchronously retrieve a list of all current journals' name. */
    public void loadAllJournalKeys(
            Callback<String[]> successCallback, Callback<Void> failureCallback) {
        assert mNativeFeedJournalBridge != 0;
        FeedJournalBridgeJni.get().loadAllJournalKeys(
                mNativeFeedJournalBridge, FeedJournalBridge.this, successCallback, failureCallback);
    }

    /** Delete all journals. Reports success or failure. */
    public void deleteAllJournals(Callback<Boolean> callback) {
        assert mNativeFeedJournalBridge != 0;
        FeedJournalBridgeJni.get().deleteAllJournals(
                mNativeFeedJournalBridge, FeedJournalBridge.this, callback);
    }

    @NativeMethods
    interface Natives {
        long init(FeedJournalBridge caller, Profile profile);
        void destroy(long nativeFeedJournalBridge, FeedJournalBridge caller);
        void loadJournal(long nativeFeedJournalBridge, FeedJournalBridge caller, String journalName,
                Callback<byte[][]> successCallback, Callback<Void> failureCallback);
        void commitJournalMutation(
                long nativeFeedJournalBridge, FeedJournalBridge caller, Callback<Boolean> callback);
        void startJournalMutation(
                long nativeFeedJournalBridge, FeedJournalBridge caller, String journalName);
        void deleteJournalMutation(long nativeFeedJournalBridge, FeedJournalBridge caller);
        void addAppendOperation(
                long nativeFeedJournalBridge, FeedJournalBridge caller, byte[] value);
        void addCopyOperation(
                long nativeFeedJournalBridge, FeedJournalBridge caller, String toJournalName);
        void addDeleteOperation(long nativeFeedJournalBridge, FeedJournalBridge caller);
        void doesJournalExist(long nativeFeedJournalBridge, FeedJournalBridge caller,
                String journalName, Callback<Boolean> successCallback,
                Callback<Void> failureCallback);
        void loadAllJournalKeys(long nativeFeedJournalBridge, FeedJournalBridge caller,
                Callback<String[]> successCallback, Callback<Void> failureCallback);
        void deleteAllJournals(
                long nativeFeedJournalBridge, FeedJournalBridge caller, Callback<Boolean> callback);
    }
}
