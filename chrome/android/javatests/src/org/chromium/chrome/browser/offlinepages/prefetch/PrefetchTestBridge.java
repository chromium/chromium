// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Prefetch test Java to native bridge. */
@JNINamespace("offline_pages::prefetch")
public class PrefetchTestBridge {
    public static void enableLimitlessPrefetching(boolean enabled) {
        PrefetchTestBridgeJni.get().enableLimitlessPrefetching(enabled);
    }
    public static boolean isLimitlessPrefetchingEnabled() {
        return PrefetchTestBridgeJni.get().isLimitlessPrefetchingEnabled();
    }
    public static void insertIntoCachedImageFetcher(String url, byte[] imageData) {
        PrefetchTestBridgeJni.get().insertIntoCachedImageFetcher(url, imageData);
    }
    public static void addCandidatePrefetchURL(String url, String title, String thumbnailUrl,
            String faviconUrl, String snippet, String attribution) {
        PrefetchTestBridgeJni.get().addCandidatePrefetchURL(
                url, title, thumbnailUrl, faviconUrl, snippet, attribution);
    }

    @NativeMethods
    interface Natives {
        void enableLimitlessPrefetching(boolean enabled);
        boolean isLimitlessPrefetchingEnabled();
        void insertIntoCachedImageFetcher(String url, byte[] imageData);
        void addCandidatePrefetchURL(String url, String title, String thumbnailUrl,
                String faviconUrl, String snippet, String attribution);
    }
}
