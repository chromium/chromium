// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import org.chromium.base.annotations.JNINamespace;

/** Prefetch test Java to native bridge. */
@JNINamespace("offline_pages::prefetch")
public class PrefetchTestBridge {
    public static void enableLimitlessPrefetching(boolean enabled) {
        nativeEnableLimitlessPrefetching(enabled);
    }
    public static boolean isLimitlessPrefetchingEnabled() {
        return nativeIsLimitlessPrefetchingEnabled();
    }
    public static void skipNTPSuggestionsAPIKeyCheck() {
        nativeSkipNTPSuggestionsAPIKeyCheck();
    }
    public static void insertIntoCachedImageFetcher(String url, byte[] imageData) {
        nativeInsertIntoCachedImageFetcher(url, imageData);
    }
    public static void addCandidatePrefetchURL(String url, String title, String thumbnailUrl,
            String faviconUrl, String snippet, String attribution) {
        nativeAddCandidatePrefetchURL(url, title, thumbnailUrl, faviconUrl, snippet, attribution);
    }

    static native void nativeEnableLimitlessPrefetching(boolean enabled);
    static native boolean nativeIsLimitlessPrefetchingEnabled();
    static native void nativeSkipNTPSuggestionsAPIKeyCheck();
    static native void nativeInsertIntoCachedImageFetcher(String url, byte[] imageData);
    static native void nativeAddCandidatePrefetchURL(String url, String title, String thumbnailUrl,
            String faviconUrl, String snippet, String attribution);
}
