// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * A ContentProvider used to push Attribution data into Chrome from other Apps.
 * This ContentProvider is append-only and does not expose any update or query capabilities.
 */
public class AttributionReportingProviderImpl extends AttributionReportingProvider.Impl {
    private static final String TAG = "AttributionReporting";

    private ImpressionPersistentStore<DataOutputStream, DataInputStream> mImpressionPersistentStore;

    public AttributionReportingProviderImpl() {
        this(new ImpressionPersistentStore<>(new ImpressionPersistentStoreFileManagerImpl()));
    }

    @VisibleForTesting
    /* package */ AttributionReportingProviderImpl(
            ImpressionPersistentStore impressionPersistentStore) {
        mImpressionPersistentStore = impressionPersistentStore;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri unused, ContentValues values) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)) {
            return null;
        }

        final String sourceEventId =
                values.getAsString(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID);
        final String destination =
                values.getAsString(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION);

        if (TextUtils.isEmpty(sourceEventId) || TextUtils.isEmpty(destination)) {
            throw new IllegalArgumentException("Missing attribution key(s).");
        }

        final String sourcePackageName = getCallingPackage();
        final String reportTo =
                values.getAsString(AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO);
        final Long expiry = values.getAsLong(AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY);

        final AttributionParameters parameters = new AttributionParameters(
                sourcePackageName, sourceEventId, destination, reportTo, expiry);

        FutureTask<Uri> insertTask = new FutureTask<>(() -> {
            if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                return null;
            }

            // TODO(https://crbug.com/1210171): Do we need to flush the stored metrics here, or
            // should we wait for a conversion event to flush?
            return insertOnUiThread(parameters);
        });

        // This cannot be BEST_EFFORT as the browser may not yet be started and so BEST_EFFORT tasks
        // will not be run.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, insertTask);
        try {
            Uri result = insertTask.get();
            if (result == null) {
                boolean flush = mImpressionPersistentStore.storeImpression(parameters);
                if (flush) scheduleStorageFlush();
                return Uri.EMPTY;
            }
            return result;
        } catch (Exception e) {
            // Note that according to documentation we can only throw IllegalArgumentException and
            // NullPointerException from ContentProviders as Android needs to be able to communicate
            // these exceptions across processes.
            // This is an internal exception, so just return null to indicate an error occurred.
            Log.e(TAG, "Internal error occured reporting attribution: ", e);
            return null;
        }
    }

    private void scheduleStorageFlush() throws ExecutionException, InterruptedException {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        long current = System.currentTimeMillis();
        long lastStartup =
                prefs.readLong(ChromePreferenceKeys.ATTRIBUTION_PROVIDER_LAST_BROWSER_START);
        if (current - lastStartup
                < TimeUnit.HOURS.toMillis(ImpressionPersistentStore.MIN_REPORTING_INTERVAL_HOURS)) {
            // We recently already started up the browser to flush storage, so avoid starting up
            // again so soon.
            return;
        }
        prefs.writeLong(ChromePreferenceKeys.ATTRIBUTION_PROVIDER_LAST_BROWSER_START, current);

        // Schedule a task to perform the storage flush in the background.
        TaskInfo taskInfo = TaskInfo.createTask(TaskIds.ATTRIBUTION_PROVIDER_FLUSH_JOB_ID,
                                            TaskInfo.OneOffInfo.create().build())
                                    .setUpdateCurrent(true)
                                    .setIsPersisted(true)
                                    .build();

        FutureTask<Void> scheduleTask = new FutureTask<Void>(() -> {
            // This will overwrite any existing task with this ID.
            BackgroundTaskSchedulerFactory.getScheduler().schedule(getContext(), taskInfo);
            return null;
        });
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, scheduleTask);
        scheduleTask.get();
    }

    private Uri insertOnUiThread(final AttributionParameters parameters) {
        AttributionMetrics.recordAttributionEvent(
                AttributionMetrics.AttributionEvent.RECEIVED_WITH_NATIVE, 1);
        AttributionReporter.getInstance().reportAppImpression(Profile.getLastUsedRegularProfile(),
                parameters.getSourcePackageName(), parameters.getSourceEventId(),
                parameters.getDestination(), parameters.getReportTo(), parameters.getExpiry(),
                parameters.getEventTime());

        // We don't have a meaningful Uri to return, so just return an empty one to indicate
        // success (in place of null for failure).
        return Uri.EMPTY;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }
}
