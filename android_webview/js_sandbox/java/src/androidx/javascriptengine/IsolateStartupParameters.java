// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import androidx.annotation.IntRange;
import androidx.annotation.RequiresFeature;

/**
 * Class used to set startup parameters for {@link JavaScriptIsolate}.
 */
public final class IsolateStartupParameters {
    private long mMaxHeapSizeBytes;
    public static final long DEFAULT_ISOLATE_HEAP_SIZE = 0;
    public IsolateStartupParameters(){};

    /**
     * Sets the max heap size used by the {@link JavaScriptIsolate}.
     *
     * A heap size of {@link IsolateStartupParameters#DEFAULT_ISOLATE_HEAP_SIZE} indicates no
     * limit.
     * <p>
     * If a value higher than the device specific maximum heap size limit is supplied, this limit
     * will be used as the maximum heap size.
     * <p>
     * The applied limit may not be exact. For example, the limit may internally be rounded up to
     * some multiple of bytes, be increased to some minimum value, or reduced to some maximum
     * supported value.
     *
     * @param size heap size in bytes
     */
    @RequiresFeature(name = JavaScriptSandbox.JS_FEATURE_ISOLATE_MAX_HEAP_SIZE,
            enforcement = "androidx.javascriptengine.JavaScriptSandbox#isFeatureSupported")
    public void
    setMaxHeapSizeBytes(@IntRange(from = 0) long size) {
        if (size < 0) {
            throw new IllegalArgumentException("maxHeapSizeBytes should be >= 0");
        }
        mMaxHeapSizeBytes = size;
    }

    /**
     * Gets the max heap size used by the {@link JavaScriptIsolate}.
     *
     * If not set using {@link IsolateStartupParameters#setMaxHeapSizeBytes(long)}, the default
     * value is {@link IsolateStartupParameters#DEFAULT_ISOLATE_HEAP_SIZE} which indicates no heap
     * size limit.
     *
     * @return heap size in bytes
     */
    public @IntRange(from = 0) long getMaxHeapSizeBytes() {
        return mMaxHeapSizeBytes;
    }
}
