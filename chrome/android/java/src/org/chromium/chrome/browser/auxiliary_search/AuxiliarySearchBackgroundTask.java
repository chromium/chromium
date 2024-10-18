// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.StreamUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.url.GURL;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Task to donate tab favicons for Auxiliary search. */
public class AuxiliarySearchBackgroundTask extends NativeBackgroundTask {
    // The result of a donation.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        DonateResult.NO_DATA,
        DonateResult.FAILED,
        DonateResult.SUCCEED,
        DonateResult.MAX_COUNT
    })
    @interface DonateResult {
        int NO_DATA = 0;
        int FAILED = 1;
        int SUCCEED = 2;
        int MAX_COUNT = 3;
    }

    private static final String SCHEDULE_DELAY_TIME_UMA =
            "Search.AuxiliarySearch.Schedule.DelayTime";
    private static final String SCHEDULE_DONATE_RESULT_UMA =
            "Search.AuxiliarySearch.Schedule.FaviconDonateResult";
    private final Map<Integer, Bitmap> mTabIdToFaviconMap = new HashMap<>();

    @NonNull private Context mContext;
    private int mTaskFinishedCount;
    @NonNull private AuxiliarySearchController mAuxiliarySearchController;

    @Override
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.AUXILIARY_SEARCH_DONATE_JOB_ID;
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context,
            TaskParameters taskParameters,
            TaskFinishedCallback taskFinishedCallback) {
        mContext = context;

        mTaskFinishedCount = 0;
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        mAuxiliarySearchController =
                AuxiliarySearchControllerFactory.createAuxiliarySearchController(
                        mContext, profile, /* TabModelSelector= */ null);

        long startTimeMs = SystemClock.currentThreadTimeMillis();
        // Record the delay from soonest expected wakeup time.
        long delayFromExpectedMs =
                startTimeMs
                        - taskParameters
                                .getExtras()
                                .getLong(AuxiliarySearchProvider.TASK_CREATED_TIME);
        RecordHistogram.recordLongTimesHistogram(SCHEDULE_DELAY_TIME_UMA, delayFromExpectedMs);

        readTabDonateMetadataAsync(
                (tabs) ->
                        onTabDonateMetadataRead(
                                profile,
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tab_grid_favicon_size),
                                startTimeMs,
                                taskFinishedCallback,
                                new FaviconHelper(),
                                mAuxiliarySearchController,
                                tabs));
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.AUXILIARY_SEARCH_DONATE_JOB_ID;

        // Native didn't complete loading, but it was supposed to.
        // Presume we need to reschedule.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.AUXILIARY_SEARCH_DONATE_JOB_ID;

        // The method is called when the task was interrupted due to some reason.
        // It is not called when the task finishes successfully. Reschedule so
        // we can attempt it again.
        return true;
    }

    /**
     * Reads the saved metadata file to get tabs.
     *
     * @param callback The callback to notify when the list of tabs is available.
     */
    @VisibleForTesting
    void readTabDonateMetadataAsync(@NonNull Callback<List<AuxiliarySearchEntry>> callback) {
        new AsyncTask<>() {
            @Override
            protected Object doInBackground() {
                File tabDonateFile = AuxiliarySearchProvider.getTabDonateFile(mContext);
                if (!tabDonateFile.exists()) {
                    return null;
                }
                FileInputStream stream = null;
                byte[] data;
                try {
                    stream = new FileInputStream(tabDonateFile);
                    data = new byte[(int) tabDonateFile.length()];
                    stream.read(data);
                } catch (IOException exception) {
                    return null;
                } finally {
                    StreamUtil.closeQuietly(stream);
                }
                return new DataInputStream(new ByteArrayInputStream(data));
            }

            @Override
            protected void onPostExecute(Object o) {
                DataInputStream stream = (DataInputStream) o;
                try {
                    callback.onResult(AuxiliarySearchProvider.readSavedMetadataFile(stream));
                    // TODO(crbug.com/370478696): Delete the metadata file after reading.
                } catch (IOException e) {
                    callback.onResult(null);
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Called when the metadata file is read. This functions will fetch the favicons for all tabs in
     * the list.
     */
    void onTabDonateMetadataRead(
            @NonNull Profile profile,
            int faviconSize,
            long startTimeMs,
            @NonNull TaskFinishedCallback taskFinishedCallback,
            @NonNull FaviconHelper faviconHelper,
            @NonNull AuxiliarySearchController auxiliarySearchController,
            @Nullable List<AuxiliarySearchEntry> tabs) {
        if (tabs == null || tabs.isEmpty()) {
            taskFinishedCallback.taskFinished(/* needsReschedule= */ false);
            return;
        }

        for (AuxiliarySearchEntry tab : tabs) {
            faviconHelper.getLocalFaviconImageForURL(
                    profile,
                    new GURL(tab.getUrl()),
                    faviconSize,
                    (bitmap, url) -> {
                        if (bitmap != null) {
                            mTabIdToFaviconMap.put(tab.getId(), bitmap);
                        }
                        mTaskFinishedCount++;
                        // Notifies the taskFinishedCallback after all favicon fetching are
                        // responded.
                        if (mTaskFinishedCount == tabs.size()) {
                            if (!mTabIdToFaviconMap.isEmpty()) {
                                auxiliarySearchController.onBackgroundTaskStart(
                                        tabs,
                                        mTabIdToFaviconMap,
                                        (success) -> {
                                            taskFinishedCallback.taskFinished(
                                                    /* needsReschedule= */ false);
                                            RecordHistogram.recordEnumeratedHistogram(
                                                    SCHEDULE_DONATE_RESULT_UMA,
                                                    success
                                                            ? DonateResult.SUCCEED
                                                            : DonateResult.FAILED,
                                                    DonateResult.MAX_COUNT);
                                        },
                                        startTimeMs);
                            } else {
                                // There isn't any favicons to donate, stops here.
                                taskFinishedCallback.taskFinished(/* needsReschedule= */ false);
                                RecordHistogram.recordEnumeratedHistogram(
                                        SCHEDULE_DONATE_RESULT_UMA,
                                        DonateResult.NO_DATA,
                                        DonateResult.MAX_COUNT);
                            }
                        }
                    });
        }
    }
}
