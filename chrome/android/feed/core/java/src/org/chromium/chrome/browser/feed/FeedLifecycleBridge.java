// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Java-side bridge that receives native-initiated lifecycle events and forwards them to
 * FeedAppLifecycle. These events include, e.g., cached data clearing and history deletion.
 */
@JNINamespace("feed")
public class FeedLifecycleBridge {
    private long mNativeBridge;

    /**
     * Creates a FeedLifecycleBridge, which is responsible for receiving and forwarding native
     * lifecycle events.
     * @param profile Profile of the user whose lifecycle events we care about.
     */
    public FeedLifecycleBridge(Profile profile) {
        mNativeBridge = nativeInit(profile);
    }

    /*
     * Cleans up native half of this bridge.
     */
    public void destroy() {
        assert mNativeBridge != 0;
        nativeDestroy(mNativeBridge);
        mNativeBridge = 0;
    }

    @CalledByNative
    private static void onCachedDataCleared() {
        FeedProcessScopeFactory.getFeedAppLifecycle().onCachedDataCleared();
    }

    // This would ordinarily be non-static, but there's no instance state, so for simplicity it's
    // isomorphic to onCachedDataCleared.
    @CalledByNative
    private static void onHistoryDeleted() {
        FeedProcessScopeFactory.getFeedAppLifecycle().onHistoryDeleted();
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedLifecycleBridge);
}