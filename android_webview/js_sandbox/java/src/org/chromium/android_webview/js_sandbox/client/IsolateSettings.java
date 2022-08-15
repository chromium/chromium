// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import androidx.annotation.RequiresFeature;

/**
 * Class used to set startup parameters for {@link JsIsolate}.
 */
public class IsolateSettings {
    private long mMaxHeapSizeBytes;
    public IsolateSettings(){};

    /**
     * Sets the max heap size used by the {@JsIsolate}.
     *
     * A heap size of 0 indicates no limit. Default value of the heap size limit is 0.
     *
     * The limit which is applied in practice may not be exact. For example, the limit may
     * internally be rounded up to some multiple of bytes, be increased to some minimum value, or
     * reduced to some maximum supported value.
     *
     * @param size heap size in bytes
     */
    @RequiresFeature(name = JsSandbox.ISOLATE_MAX_HEAP_SIZE,
            enforcement =
                    "org.chromium.android_webview.js_sandbox.client.JsSandbox#isFeatureSupported")
    public void
    setMaxHeapSizeBytes(long size) {
        if (size < 0) {
            throw new IllegalArgumentException("maxHeapSizeBytes should be >= 0");
        }
        mMaxHeapSizeBytes = size;
    }

    /**
     * Gets the max heap size used by the {@JsIsolate}.
     *
     * If not set using {@link IsolateSettings#setMaxHeapSizeBytes(long)}, the default value is 0
     * which indicates no heap size limit.
     *
     * @return heap size in bytes
     */
    public long getMaxHeapSizeBytes() {
        return mMaxHeapSizeBytes;
    }
}
