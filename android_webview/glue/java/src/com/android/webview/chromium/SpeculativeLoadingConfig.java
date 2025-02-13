// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;

@Lifetime.Temporary
public class SpeculativeLoadingConfig {

    /** Represents the maximum number of cached prefetched that can exist at a time. */
    public final int maxPrefetches;

    /** The Time-To-Live in seconds for the prefetch request inside the prefetch cache. */
    public final int prefetchTTLSeconds;

    /** The max amount of prerenders that can live in the cache. */
    public final int maxPrerenders;

    public SpeculativeLoadingConfig(int maxPrefetches, int prefetchTTLSeconds, int maxPrerenders) {
        this.maxPrefetches = maxPrefetches;
        this.prefetchTTLSeconds = prefetchTTLSeconds;
        this.maxPrerenders = maxPrerenders;
    }
}
