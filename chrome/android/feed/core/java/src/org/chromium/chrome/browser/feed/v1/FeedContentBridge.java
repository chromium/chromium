// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Delete;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.DeleteByPrefix;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Upsert;
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
        mNativeFeedContentBridge = FeedContentBridgeJni.get().init(FeedContentBridge.this, profile);
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
        FeedContentBridgeJni.get().destroy(mNativeFeedContentBridge, FeedContentBridge.this);
        mNativeFeedContentBridge = 0;
    }

    public void loadContent(List<String> keys, Callback<Map<String, byte[]>> successCallback,
            Callback<Void> failureCallback) {
        assert mNativeFeedContentBridge != 0;
        String[] keysArray = keys.toArray(new String[keys.size()]);
        FeedContentBridgeJni.get().loadContent(mNativeFeedContentBridge, FeedContentBridge.this,
                keysArray, successCallback, failureCallback);
    }

    public void loadContentByPrefix(String prefix, Callback<Map<String, byte[]>> successCallback,
            Callback<Void> failureCallback) {
        assert mNativeFeedContentBridge != 0;
        FeedContentBridgeJni.get().loadContentByPrefix(mNativeFeedContentBridge,
                FeedContentBridge.this, prefix, successCallback, failureCallback);
    }

    public void loadAllContentKeys(
            Callback<String[]> successCallback, Callback<Void> failureCallback) {
        assert mNativeFeedContentBridge != 0;
        FeedContentBridgeJni.get().loadAllContentKeys(
                mNativeFeedContentBridge, FeedContentBridge.this, successCallback, failureCallback);
    }

    public void commitContentMutation(ContentMutation contentMutation, Callback<Boolean> callback) {
        assert mNativeFeedContentBridge != 0;

        FeedContentBridgeJni.get().createContentMutation(
                mNativeFeedContentBridge, FeedContentBridge.this);
        for (ContentOperation operation : contentMutation.getOperations()) {
            switch (operation.getType()) {
                case Type.UPSERT:
                    Upsert upsert = (Upsert) operation;
                    FeedContentBridgeJni.get().appendUpsertOperation(mNativeFeedContentBridge,
                            FeedContentBridge.this, upsert.getKey(), upsert.getValue());
                    break;
                case Type.DELETE:
                    Delete delete = (Delete) operation;
                    FeedContentBridgeJni.get().appendDeleteOperation(
                            mNativeFeedContentBridge, FeedContentBridge.this, delete.getKey());
                    break;
                case Type.DELETE_BY_PREFIX:
                    DeleteByPrefix deleteByPrefix = (DeleteByPrefix) operation;
                    FeedContentBridgeJni.get().appendDeleteByPrefixOperation(
                            mNativeFeedContentBridge, FeedContentBridge.this,
                            deleteByPrefix.getPrefix());
                    break;
                case Type.DELETE_ALL:
                    FeedContentBridgeJni.get().appendDeleteAllOperation(
                            mNativeFeedContentBridge, FeedContentBridge.this);
                    break;
                default:
                    // Operation type is not supported, therefore failing immediately.
                    assert false : "Unsupported type of operation.";
                    FeedContentBridgeJni.get().deleteContentMutation(
                            mNativeFeedContentBridge, FeedContentBridge.this);
                    callback.onResult(false);
                    return;
            }
        }
        FeedContentBridgeJni.get().commitContentMutation(
                mNativeFeedContentBridge, FeedContentBridge.this, callback);
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

    @NativeMethods
    interface Natives {
        long init(FeedContentBridge caller, Profile profile);
        void destroy(long nativeFeedContentBridge, FeedContentBridge caller);
        void loadContent(long nativeFeedContentBridge, FeedContentBridge caller, String[] keys,
                Callback<Map<String, byte[]>> successCallback, Callback<Void> failureCallback);
        void loadContentByPrefix(long nativeFeedContentBridge, FeedContentBridge caller,
                String prefix, Callback<Map<String, byte[]>> successCallback,
                Callback<Void> failureCallback);
        void loadAllContentKeys(long nativeFeedContentBridge, FeedContentBridge caller,
                Callback<String[]> successCallback, Callback<Void> failureCallback);
        void commitContentMutation(
                long nativeFeedContentBridge, FeedContentBridge caller, Callback<Boolean> callback);
        void createContentMutation(long nativeFeedContentBridge, FeedContentBridge caller);
        void deleteContentMutation(long nativeFeedContentBridge, FeedContentBridge caller);
        void appendDeleteOperation(
                long nativeFeedContentBridge, FeedContentBridge caller, String key);
        void appendDeleteByPrefixOperation(
                long nativeFeedContentBridge, FeedContentBridge caller, String prefix);
        void appendUpsertOperation(
                long nativeFeedContentBridge, FeedContentBridge caller, String key, byte[] data);
        void appendDeleteAllOperation(long nativeFeedContentBridge, FeedContentBridge caller);
    }
}
