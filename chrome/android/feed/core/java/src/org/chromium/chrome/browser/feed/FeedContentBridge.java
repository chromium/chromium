// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.storage.ContentMutation;
import com.google.android.libraries.feed.host.storage.ContentOperation;
import com.google.android.libraries.feed.host.storage.ContentOperation.Delete;
import com.google.android.libraries.feed.host.storage.ContentOperation.DeleteByPrefix;
import com.google.android.libraries.feed.host.storage.ContentOperation.Type;
import com.google.android.libraries.feed.host.storage.ContentOperation.Upsert;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Provides access to native implementations of content storage.
 */
@JNINamespace("feed")
public class FeedContentBridge {
    private long mNativeFeedContentBridge;

    /**
     * Creates a {@link FeedContentBridge} for accessing native content storage
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedContentBridge(Profile profile) {
        mNativeFeedContentBridge = nativeInit(profile);
    }

    /**
     * Creates a {@link FeedContentBridge} for accessing native content storage
     * implementation for the current user, and initial native side bridge.
     */
    @VisibleForTesting
    public FeedContentBridge() {}

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedContentBridge != 0;
        nativeDestroy(mNativeFeedContentBridge);
        mNativeFeedContentBridge = 0;
    }

    public void loadContent(List<String> keys, Callback<Map<String, byte[]>> successCallback,
            Callback<Void> failureCallback) {
        assert mNativeFeedContentBridge != 0;
        String[] keysArray = keys.toArray(new String[keys.size()]);
        nativeLoadContent(mNativeFeedContentBridge, keysArray, successCallback, failureCallback);
    }

    public void loadContentByPrefix(String prefix, Callback<Map<String, byte[]>> successCallback,
            Callback<Void> failureCallback) {
        assert mNativeFeedContentBridge != 0;
        nativeLoadContentByPrefix(
                mNativeFeedContentBridge, prefix, successCallback, failureCallback);
    }

    public void loadAllContentKeys(
            Callback<String[]> successCallback, Callback<Void> failureCallback) {
        assert mNativeFeedContentBridge != 0;
        nativeLoadAllContentKeys(mNativeFeedContentBridge, successCallback, failureCallback);
    }

    public void commitContentMutation(ContentMutation contentMutation, Callback<Boolean> callback) {
        assert mNativeFeedContentBridge != 0;

        nativeCreateContentMutation(mNativeFeedContentBridge);
        for (ContentOperation operation : contentMutation.getOperations()) {
            switch (operation.getType()) {
                case Type.UPSERT:
                    Upsert upsert = (Upsert) operation;
                    nativeAppendUpsertOperation(
                            mNativeFeedContentBridge, upsert.getKey(), upsert.getValue());
                    break;
                case Type.DELETE:
                    Delete delete = (Delete) operation;
                    nativeAppendDeleteOperation(mNativeFeedContentBridge, delete.getKey());
                    break;
                case Type.DELETE_BY_PREFIX:
                    DeleteByPrefix deleteByPrefix = (DeleteByPrefix) operation;
                    nativeAppendDeleteByPrefixOperation(
                            mNativeFeedContentBridge, deleteByPrefix.getPrefix());
                    break;
                case Type.DELETE_ALL:
                    nativeAppendDeleteAllOperation(mNativeFeedContentBridge);
                    break;
                default:
                    // Operation type is not supported, therefore failing immediately.
                    assert false : "Unsupported type of operation.";
                    nativeDeleteContentMutation(mNativeFeedContentBridge);
                    callback.onResult(false);
                    return;
            }
        }
        nativeCommitContentMutation(mNativeFeedContentBridge, callback);
    }

    @CalledByNative
    private static Object createKeyAndDataMap(String[] keys, byte[][] data) {
        assert keys.length == data.length;
        Map<String, byte[]> valueMap = new HashMap<>(keys.length);
        for (int i = 0; i < keys.length && i < data.length; ++i) {
            valueMap.put(keys[i], data[i]);
        }
        return valueMap;
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedContentBridge);
    private native void nativeLoadContent(long nativeFeedContentBridge, String[] keys,
            Callback<Map<String, byte[]>> successCallback, Callback<Void> failureCallback);
    private native void nativeLoadContentByPrefix(long nativeFeedContentBridge, String prefix,
            Callback<Map<String, byte[]>> successCallback, Callback<Void> failureCallback);
    private native void nativeLoadAllContentKeys(long nativeFeedContentBridge,
            Callback<String[]> successCallback, Callback<Void> failureCallback);
    private native void nativeCommitContentMutation(
            long nativeFeedContentBridge, Callback<Boolean> callback);
    private native void nativeCreateContentMutation(long nativeFeedContentBridge);
    private native void nativeDeleteContentMutation(long nativeFeedContentBridge);
    private native void nativeAppendDeleteOperation(long nativeFeedContentBridge, String key);
    private native void nativeAppendDeleteByPrefixOperation(
            long nativeFeedContentBridge, String prefix);
    private native void nativeAppendUpsertOperation(
            long nativeFeedContentBridge, String key, byte[] data);
    private native void nativeAppendDeleteAllOperation(long nativeFeedContentBridge);
}
