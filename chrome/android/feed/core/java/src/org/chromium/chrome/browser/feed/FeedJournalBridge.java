// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.storage.JournalMutation;
import com.google.android.libraries.feed.host.storage.JournalOperation;
import com.google.android.libraries.feed.host.storage.JournalOperation.Append;
import com.google.android.libraries.feed.host.storage.JournalOperation.Copy;
import com.google.android.libraries.feed.host.storage.JournalOperation.Type;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
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
        mNativeFeedJournalBridge = nativeInit(profile);
    }

    /**
     * Creates a {@link FeedJournalBridge} for testing.
     */
    @VisibleForTesting
    public FeedJournalBridge() {}

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedJournalBridge != 0;
        nativeDestroy(mNativeFeedJournalBridge);
        mNativeFeedJournalBridge = 0;
    }

    /** Loads the journal and asynchronously returns the contents. */
    public void loadJournal(String journalName, Callback<byte[][]> successCallback,
            Callback<Void> failureCallback) {
        assert mNativeFeedJournalBridge != 0;
        nativeLoadJournal(mNativeFeedJournalBridge, journalName, successCallback, failureCallback);
    }

    /**
     * Commits the operations in {@link JournalMutation} in order and asynchronously reports the
     * {@link CommitResult}. If all the operations succeed, {@code callback} is called with a
     * success result. If any operation fails, {@code callback} is called with a failure result and
     * the remaining operations are not processed.
     */
    public void commitJournalMutation(JournalMutation mutation, Callback<Boolean> callback) {
        assert mNativeFeedJournalBridge != 0;

        nativeStartJournalMutation(mNativeFeedJournalBridge, mutation.getJournalName());
        for (JournalOperation operation : mutation.getOperations()) {
            switch (operation.getType()) {
                case Type.APPEND:
                    Append append = (Append) operation;
                    nativeAddAppendOperation(mNativeFeedJournalBridge, append.getValue());
                    break;
                case Type.COPY:
                    Copy copy = (Copy) operation;
                    nativeAddCopyOperation(mNativeFeedJournalBridge, copy.getToJournalName());
                    break;
                case Type.DELETE:
                    nativeAddDeleteOperation(mNativeFeedJournalBridge);
                    break;
                default:
                    // Operation type is not supported, therefore failing immediately.
                    assert false : "Unsupported type of operation.";
                    nativeDeleteJournalMutation(mNativeFeedJournalBridge);
                    callback.onResult(false);
                    return;
            }
        }
        nativeCommitJournalMutation(mNativeFeedJournalBridge, callback);
    }

    /** Determines whether a journal exists and asynchronously responds. */
    public void doesJournalExist(
            String journalName, Callback<Boolean> successCallback, Callback<Void> failureCallback) {
        assert mNativeFeedJournalBridge != 0;
        nativeDoesJournalExist(
                mNativeFeedJournalBridge, journalName, successCallback, failureCallback);
    }

    /** Asynchronously retrieve a list of all current journals' name. */
    public void loadAllJournalKeys(
            Callback<String[]> successCallback, Callback<Void> failureCallback) {
        assert mNativeFeedJournalBridge != 0;
        nativeLoadAllJournalKeys(mNativeFeedJournalBridge, successCallback, failureCallback);
    }

    /** Delete all journals. Reports success or failure. */
    public void deleteAllJournals(Callback<Boolean> callback) {
        assert mNativeFeedJournalBridge != 0;
        nativeDeleteAllJournals(mNativeFeedJournalBridge, callback);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedJournalBridge);
    private native void nativeLoadJournal(long nativeFeedJournalBridge, String journalName,
            Callback<byte[][]> successCallback, Callback<Void> failureCallback);
    private native void nativeCommitJournalMutation(
            long nativeFeedJournalBridge, Callback<Boolean> callback);
    private native void nativeStartJournalMutation(
            long nativeFeedJournalBridge, String journalName);
    private native void nativeDeleteJournalMutation(long nativeFeedJournalBridge);
    private native void nativeAddAppendOperation(long nativeFeedJournalBridge, byte[] value);
    private native void nativeAddCopyOperation(long nativeFeedJournalBridge, String toJournalName);
    private native void nativeAddDeleteOperation(long nativeFeedJournalBridge);
    private native void nativeDoesJournalExist(long nativeFeedJournalBridge, String journalName,
            Callback<Boolean> successCallback, Callback<Void> failureCallback);
    private native void nativeLoadAllJournalKeys(long nativeFeedJournalBridge,
            Callback<String[]> successCallback, Callback<Void> failureCallback);
    private native void nativeDeleteAllJournals(
            long nativeFeedJournalBridge, Callback<Boolean> callback);
}
