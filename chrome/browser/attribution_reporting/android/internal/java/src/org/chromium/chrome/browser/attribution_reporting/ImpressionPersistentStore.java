// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.util.Pair;

import org.chromium.base.Log;
import org.chromium.chrome.browser.attribution_reporting.ImpressionPersistentStoreFileManager.FileProperties;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.io.Closeable;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.EOFException;
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

    // 50k ought be enough for anyone. This value is mostly intended to prevent spam while Chrome
    // is backgrounded, and isn't intended to set limits on how many impressions apps can report
    // under normal circumstances.
    // TODO(https://crbug.com/1210171): Figure out what a reasonable value here is. Should this be
    // controllable by finch?
    // TODO(https://crbug.com/1210171): Periodically flush the storage to enable cross-device
    // attribution and ensure more recent attributions aren't prevented by stale attributions if
    // browser hasn't been launched in a while.
    /* package */ static final long MAX_STORAGE_BYTES_PER_PACKAGE =
            50 * ConversionUtils.BYTES_PER_KILOBYTE;

    // Shared lock across all files because getAndClearStoredImpressions() reads then deletes all
    // files. Very unlikely to be contended as multiple packages will rarely report impressions at
    // the same time.
    private static final Object sFileLock = new Object();

    private final ImpressionPersistentStoreFileManager<W, R> mFileManager;

    public ImpressionPersistentStore(ImpressionPersistentStoreFileManager<W, R> fileManager) {
        mFileManager = fileManager;
    }

    public void storeImpression(final AttributionParameters parameters) {
        synchronized (sFileLock) {
            W stream = null;
            try {
                Pair<W, Long> filePair =
                        mFileManager.getForPackage(parameters.getSourcePackageName(), VERSION);
                stream = filePair.first;
                long fileSize = filePair.second;

                // TODO(https://crbug.com/1210171): Record metrics for dropped impressions.
                if (fileSize >= MAX_STORAGE_BYTES_PER_PACKAGE) return;
                stream.writeUTF(parameters.getSourceEventId());
                stream.writeUTF(parameters.getDestination());
                stream.writeUTF(parameters.getReportTo() == null ? "" : parameters.getReportTo());
                stream.writeLong(parameters.getExpiry());
                // Store the time of the attribution report so that when the attribution gets
                // processed we know when it was originally reported (for expiry/ordering/etc.
                // purposes).
                stream.writeLong(System.currentTimeMillis());
                stream.writeChar(SENTINEL);
            } catch (Exception e) {
                Log.w(TAG, WRITE_FAILURE, e);
            } finally {
                try {
                    if (stream != null) stream.close();
                } catch (Exception e) {
                    Log.w(TAG, WRITE_FAILURE, e);
                }
            }
        }
    }

    private void readImpressions(List<AttributionParameters> output, FileProperties<R> properties) {
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
                    // TODO(https://crbug.com/1210171): Record metrics for dropped impressions.
                    Log.w(TAG, "Failed to read Impression data, data was corrupted.");
                    return;
                }
                AttributionParameters params =
                        AttributionParameters.forCachedEvent(properties.packageName, sourceEventId,
                                destination, reportTo, expiry, eventTime);
                output.add(params);
            }
        } catch (Exception e) {
            // TODO(https://crbug.com/1210171): Record metrics for dropped impressions.
            Log.w(TAG, READ_FAILURE, e);
        } finally {
            try {
                properties.reader.close();
            } catch (Exception e) {
                Log.w(TAG, READ_FAILURE, e);
            }
        }
    }

    public List<AttributionParameters> getAndClearStoredImpressions() {
        List<AttributionParameters> parameters = new ArrayList<>();
        synchronized (sFileLock) {
            try {
                for (FileProperties<R> properties : mFileManager.getAllFiles()) {
                    readImpressions(parameters, properties);
                }
            } catch (Exception e) {
                Log.w(TAG, READ_FAILURE, e);
            }
            mFileManager.clearAllData();
        }
        return parameters;
    }
}
