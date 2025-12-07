// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.StreamUtil;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchHostType;
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
@NullMarked
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

    private Context mContext;
    private int mTaskFinishedCount;
    private @Nullable AuxiliarySearchController mAuxiliarySearchController;
    private FaviconHelper mFaviconHelper;

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
        // The AuxiliarySearchControllerFactory#setIsTablet() must be called before creating the
        // controller which checks AuxiliarySearchControllerFactory#isEnabled(). This task won't be
        // scheduled if the device isn't a tablet.
        AuxiliarySearchControllerFactory.getInstance().setIsTablet(true);
        mAuxiliarySearchController =
                AuxiliarySearchControllerFactory.getInstance()
                        .createAuxiliarySearchController(
                                mContext,
                                profile,
                                /* tabModelSelector= */ null,
                                AuxiliarySearchHostType.BACKGROUND_TASK);

        long startTimeMs = TimeUtils.uptimeMillis();
        // Record the delay from soonest expected wakeup time.
        long delayFromExpectedMs =
                startTimeMs
                        - taskParameters
                                .getExtras()
                                .getLong(AuxiliarySearchProvider.TASK_CREATED_TIME);
        AuxiliarySearchMetrics.recordScheduledDelayTime(delayFromExpectedMs);

        int faviconSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size);

        mFaviconHelper = new FaviconHelper();
        if (mAuxiliarySearchController != null) {
            readDonationMetadataAsync(
                    mContext,
                    (tabs) ->
                            onDonationMetadataRead(
                                    profile,
                                    faviconSize,
                                    startTimeMs,
                                    taskFinishedCallback,
                                    mFaviconHelper,
                                    mAuxiliarySearchController,
                                    tabs));
        }
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
     * Reads the saved metadata file.
     *
     * @param context The application context.
     * @param callback The callback to notify when the list of data is available.
     * @param <T> The type of the entry data for donation.
     */
    @VisibleForTesting
    static <T> void readDonationMetadataAsync(
            Context context, Callback<@Nullable List<T>> callback) {
        new AsyncTask<@Nullable DataInputStream>() {
            @Override
            protected @Nullable DataInputStream doInBackground() {
                File tabDonateFile = AuxiliarySearchUtils.getTabDonateFile(context);
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
            protected void onPostExecute(@Nullable DataInputStream stream) {
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
     * Called when the metadata file is read. This functions will fetch the favicons for all entries
     * in the list.
     *
     * @param <T> The type of the entry data for donation.
     */
    @VisibleForTesting
    <T> void onDonationMetadataRead(
            Profile profile,
            int faviconSize,
            long startTimeMs,
            TaskFinishedCallback taskFinishedCallback,
            FaviconHelper faviconHelper,
            @Nullable AuxiliarySearchController auxiliarySearchController,
            @Nullable List<T> entries) {
        if (entries == null || entries.isEmpty() || auxiliarySearchController == null) {
            onTaskFinished(taskFinishedCallback);
            return;
        }

        Map<T, Bitmap> entriesToFaviconMap = new HashMap<>();
        for (T entry : entries) {
            GURL entryUrl;
            if (entry instanceof AuxiliarySearchEntry tab) {
                entryUrl = new GURL(tab.getUrl());
            } else {
                entryUrl = ((AuxiliarySearchDataEntry) entry).url;
            }
            faviconHelper.getLocalFaviconImageForURL(
                    profile,
                    entryUrl,
                    faviconSize,
                    (bitmap, url) -> {
                        if (bitmap != null) {
                            entriesToFaviconMap.put(entry, bitmap);
                        }
                        mTaskFinishedCount++;
                        // Notifies the taskFinishedCallback after all favicon fetching are
                        // responded.
                        if (mTaskFinishedCount == entries.size()) {
                            long currentTimeMs = TimeUtils.uptimeMillis();
                            AuxiliarySearchMetrics.recordScheduledFaviconFetchDuration(
                                    currentTimeMs - startTimeMs);

                            if (!entriesToFaviconMap.isEmpty()) {
                                int size = entriesToFaviconMap.size();
                                auxiliarySearchController.onBackgroundTaskStart(
                                        entries,
                                        entriesToFaviconMap,
                                        (success) -> {
                                            onTaskFinished(taskFinishedCallback);
                                            AuxiliarySearchMetrics.recordScheduledDonationResult(
                                                    success
                                                            ? DonateResult.SUCCEED
                                                            : DonateResult.FAILED);
                                        },
                                        currentTimeMs);

                                AuxiliarySearchMetrics.recordScheduledFaviconDonateCount(size);
                            } else {
                                // There isn't any favicons to donate, stops here.
                                onTaskFinished(taskFinishedCallback);
                                AuxiliarySearchMetrics.recordScheduledDonationResult(
                                        DonateResult.NO_DATA);
                            }
                        }
                    });
        }
    }

    @VisibleForTesting
    public void onTaskFinished(TaskFinishedCallback taskFinishedCallback) {
        PostTask.runOrPostTask(TaskTraits.UI_TRAITS_START, () -> destroy());
        taskFinishedCallback.taskFinished(/* needsReschedule= */ false);
    }

    private void destroy() {
        if (mAuxiliarySearchController != null) {
            mAuxiliarySearchController.destroy(/* lifecycleDispatcher= */ null);
            mAuxiliarySearchController = null;
        }
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
        }
    }
}
