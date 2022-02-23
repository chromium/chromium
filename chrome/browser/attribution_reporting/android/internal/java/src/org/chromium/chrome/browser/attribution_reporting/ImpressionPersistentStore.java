// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.util.Pair;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.attribution_reporting.ImpressionPersistentStoreFileManager.AttributionFileProperties;
import org.chromium.chrome.browser.attribution_reporting.ImpressionPersistentStoreFileManager.CachedEnumMetric;

import java.io.Closeable;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.EOFException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Temporarily caches Impressions to a file to avoid having to start up the Browser for every
 * received Attribution.
 *
 * Templating is used here to abstract away the actual classes (eg DataOutputStream) that do the
 * file writing as they're not mockable in tests as they use final methods everywhere.
 *
 * @param <W> the class attributions are written out to (eg. DataOutputStream).
 * @param <R> the class attributions are read from (eg. DataInputStream).
 */
public class ImpressionPersistentStore<W extends DataOutput & Closeable, R
                                               extends DataInput & Closeable> {
    private static final String TAG = "ImpressionStore";
    private static final String READ_FAILURE = "Failed to read Impression data.";
    private static final String WRITE_FAILURE = "Failed to write Impression data.";

    // Version number of the storage schema.
    //
    // Version 0 - 2021/09/15 - https://crrev.com/c/3152895
    /* package */ static final int VERSION = 0;

    // Sentinel used to detect file truncations.
    /* package */ static final char SENTINEL = '\0';

    // The minimum interval at which we allow the browser to be started in the background in order
    // to report attribution events.
    // TODO(https://crbug.com/1210171): Figure out what a reasonable value here is. Should this be
    // controllable by finch?
    /* package */ static final long MIN_REPORTING_INTERVAL_HOURS = 2;

    // An estimate of the size of each attribution.
    private static final long BYTES_PER_ATTRIBUTION_ESTIMATE = 500;

    // The maximum number of impressions per hour beyond which any more may be considered spam and
    // discarded.
    private static final long MAX_REPORTS_PER_PACKAGE_PER_HOUR = 250;

    // This value is mostly intended to prevent spam while Chrome  is backgrounded, and isn't
    // intended to set limits on how many impressions apps can report under normal circumstances.
    // TODO(https://crbug.com/1210171): Should this be controllable by finch?
    // TODO(https://crbug.com/1210171): Periodically flush the storage to enable cross-device
    // attribution.
    /* package */ static final long MAX_STORAGE_BYTES_PER_PACKAGE = MIN_REPORTING_INTERVAL_HOURS
            * MAX_REPORTS_PER_PACKAGE_PER_HOUR * BYTES_PER_ATTRIBUTION_ESTIMATE;

    // Flushing the storage is async so allow some additional attributions to be inserted while
    // waiting for the storage to be flushed.
    /* package */ static final double STORAGE_FLUSH_THRESHOLD = MAX_STORAGE_BYTES_PER_PACKAGE * 0.8;

    // Shared lock across all files because getAndClearStoredImpressions() reads then deletes all
    // files. Very unlikely to be contended as multiple packages will rarely report impressions at
    // the same time.
    private static final Object sFileLock = new Object();

    private final ImpressionPersistentStoreFileManager<W, R> mFileManager;

    public ImpressionPersistentStore(ImpressionPersistentStoreFileManager<W, R> fileManager) {
        mFileManager = fileManager;
    }

    /**
     * Persists the provided {@link AttributionParameters} to a file.
     * @return true if the storage is nearly full (or full) and should be flushed.
     */
    public boolean storeImpression(final AttributionParameters parameters) {
        synchronized (sFileLock) {
            W stream = null;
            try {
                Pair<W, Long> filePair =
                        mFileManager.getForPackage(parameters.getSourcePackageName(), VERSION);
                stream = filePair.first;
                long fileSize = filePair.second;

                if (fileSize >= MAX_STORAGE_BYTES_PER_PACKAGE) {
                    cacheAttributionEvent(AttributionMetrics.AttributionEvent.DROPPED_STORAGE_FULL);
                    return true;
                }

                stream.writeUTF(parameters.getSourceEventId());
                stream.writeUTF(parameters.getDestination());
                stream.writeUTF(parameters.getReportTo() == null ? "" : parameters.getReportTo());
                stream.writeLong(parameters.getExpiry());
                // Store the time of the attribution report so that when the attribution gets
                // processed we know when it was originally reported (for expiry/ordering/etc.
                // purposes).
                stream.writeLong(System.currentTimeMillis());
                stream.writeChar(SENTINEL);
                cacheAttributionEvent(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE);
                return fileSize + BYTES_PER_ATTRIBUTION_ESTIMATE >= STORAGE_FLUSH_THRESHOLD;
            } catch (Exception e) {
                cacheAttributionEvent(AttributionMetrics.AttributionEvent.DROPPED_WRITE_FAILED);
                Log.w(TAG, WRITE_FAILURE, e);
                return false;
            } finally {
                try {
                    if (stream != null) stream.close();
                } catch (Exception e) {
                    cacheAttributionEvent(AttributionMetrics.AttributionEvent.FILE_CLOSE_FAILED);
                    Log.w(TAG, WRITE_FAILURE, e);
                }
            }
        }
    }

    private void readImpressions(
            List<AttributionParameters> output, AttributionFileProperties<R> properties) {
        try {
            // If user downgraded Chrome and the schema changed, just discard the attributions.
            if (properties.version > VERSION) return;
            while (true) {
                String sourceEventId;
                try {
                    sourceEventId = properties.reader.readUTF();
                } catch (EOFException e) {
                    // This is the best way to detect the end of the file in Java -
                    // available() is an estimate and not implemented on all platforms.
                    return;
                }
                String destination = properties.reader.readUTF();
                String reportTo = properties.reader.readUTF();
                long expiry = properties.reader.readLong();
                long eventTime = properties.reader.readLong();
                char sentinel = properties.reader.readChar();
                if (sentinel != SENTINEL) {
                    // Note that metrics for failed reads are captured in
                    // getAndClearStoredImpressions().
                    Log.w(TAG, "Failed to read Impression data, data was corrupted.");
                    return;
                }
                AttributionParameters params =
                        AttributionParameters.forCachedEvent(properties.packageName, sourceEventId,
                                destination, reportTo, expiry, eventTime);
                output.add(params);
            }
        } catch (Exception e) {
            Log.w(TAG, READ_FAILURE, e);
        } finally {
            try {
                properties.reader.close();
            } catch (Exception e) {
                Log.w(TAG, READ_FAILURE, e);
            }
        }
    }

    public void cacheAttributionEvent(@AttributionMetrics.AttributionEvent int event) {
        try {
            synchronized (sFileLock) {
                mFileManager.incrementEnumMetric(AttributionMetrics.ATTRIBUTION_EVENTS_NAME, event);
            }
        } catch (IOException e) {
        }
    }

    public List<AttributionParameters> getAndClearStoredImpressions() {
        ThreadUtils.assertOnBackgroundThread();
        List<AttributionParameters> parameters = new ArrayList<>();
        int cachedAttributions = -1;
        synchronized (sFileLock) {
            try {
                for (CachedEnumMetric metric : mFileManager.getCachedEnumMetrics()) {
                    if (AttributionMetrics.isValidAttributionEventMetric(
                                metric.metricName, metric.enumValue)) {
                        if (metric.enumValue
                                == AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE) {
                            cachedAttributions = metric.count;
                        }
                        AttributionMetrics.recordAttributionEvent(metric.enumValue, metric.count);
                    } else {
                        // Drop unrecognized metrics, probably caused by version skew.
                    }
                }
            } catch (Exception e) {
                Log.w(TAG, "Failed to record Attribution metrics.", e);
            }
            try {
                for (AttributionFileProperties<R> properties :
                        mFileManager.getAllAttributionFiles()) {
                    readImpressions(parameters, properties);
                }
            } catch (Exception e) {
                Log.w(TAG, READ_FAILURE, e);
            }
            mFileManager.clearAllData();
        }
        if (cachedAttributions > parameters.size()) {
            AttributionMetrics.recordAttributionEvent(
                    AttributionMetrics.AttributionEvent.DROPPED_READ_FAILED,
                    cachedAttributions - parameters.size());
        }
        return parameters;
    }
}
