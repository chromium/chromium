// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.xsurface.PersistentKeyValueCache;

/** Implementation of xsurface's PersistentKeyValueCache. */
@JNINamespace("feed")
@NullMarked
public class FeedPersistentKeyValueCache implements PersistentKeyValueCache {
    @Override
    public void lookup(byte[] key, ValueConsumer consumer) {
        assert ThreadUtils.runningOnUiThread();
        FeedPersistentKeyValueCacheJni.get()
                .lookup(
                        key,
                        new Callback<byte[]>() {
                            @Override
                            public void onResult(byte[] result) {
                                consumer.run(result);
                            }
                        });
    }

    @Override
    public void put(byte[] key, byte[] value, @Nullable Runnable onComplete) {
        assert ThreadUtils.runningOnUiThread();
        FeedPersistentKeyValueCacheJni.get().put(key, value, onComplete);
    }

    @Override
    public void evict(byte[] key, @Nullable Runnable onComplete) {
        assert ThreadUtils.runningOnUiThread();
        FeedPersistentKeyValueCacheJni.get().evict(key, onComplete);
    }

    @NativeMethods
    interface Natives {
        void lookup(byte[] key, Object consumer);

        void put(byte[] key, byte[] value, @Nullable Runnable onComplete);

        void evict(byte[] key, @Nullable Runnable onComplete);
    }
}
