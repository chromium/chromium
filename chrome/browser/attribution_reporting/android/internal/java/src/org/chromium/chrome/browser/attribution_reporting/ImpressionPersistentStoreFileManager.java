// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.util.Pair;

import java.io.Closeable;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.util.List;

/**
 * Separates file management logic out of the ImpressionPersistentStore to allow for unit testing.
 *
 * @param <W> the class attributions are written out to (eg. DataOutputStream).
 * @param <R> the class attributions are read from (eg. DataInputStream).
 */
public interface ImpressionPersistentStoreFileManager<W extends DataOutput & Closeable, R
                                                              extends DataInput & Closeable> {
    public static class AttributionFileProperties<R> {
        public AttributionFileProperties(R reader, String packageName, int version) {
            this.reader = reader;
            this.packageName = packageName;
            this.version = version;
        }

        public R reader;
        public String packageName;
        public int version;
    }

    public static class CachedEnumMetric {
        public CachedEnumMetric(String metricName, int enumValue, int count) {
            this.metricName = metricName;
            this.enumValue = enumValue;
            this.count = count;
        }

        public String metricName;
        public int enumValue;
        public int count;
    }

    /**
     * @param packageName tab packagename of the app that sent the Attribution.
     * @param version the schema version you would like the writer for.
     * @return A pair of the {@link DataOutputStream} serialized AttributionParameters are stored
     *         in, and the current filesize of the file backing the output stream.
     *         The caller is responsible for closing the DataOutputStream.
     */
    public Pair<W, Long> getForPackage(String packageName, int version) throws IOException;

    /**
     * @return The {@link FileProperties} for each file and the package the file stores Attribution
     *         data for. The caller is responsible for closing the DataInputStream.
     */
    public List<AttributionFileProperties<R>> getAllAttributionFiles() throws IOException;

    /**
     * Increments by one the cached value for the specified metric.
     */
    public void incrementEnumMetric(String metricName, int enumValue) throws IOException;

    /**
     * Returns the current value of all cached enum metrics.
     */
    public List<CachedEnumMetric> getCachedEnumMetrics() throws IOException;

    /**
     * Clear all persisted Attribution Data files.
     */
    public void clearAllData();
}
