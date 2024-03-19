// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Class for launching the DownloadActivity and monitoring its status so dialogs could be shown on
 * top of it.
 */
public class DownloadActivityLauncher implements ApplicationStatus.ActivityStateListener {
    private static DownloadActivityLauncher sInstance;
    private static final String EXTRA_SHOW_PREFETCHED_CONTENT =
            "org.chromium.chrome.browser.download.SHOW_PREFETCHED_CONTENT";
    private @DownloadActivityStatus int mActivityStatus = DownloadActivityStatus.NOT_CREATED;
    private List<Callback<Activity>> mActivityCallbacks = new ArrayList();

    /** A set of states that represent the last state change of an Activity. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        DownloadActivityStatus.NOT_CREATED,
        DownloadActivityStatus.INITIALIZING,
        DownloadActivityStatus.STARTED,
    })
    public @interface DownloadActivityStatus {
        /** DownloadActivity is not created. */
        int NOT_CREATED = 0;

        /** StartActivity() has been called. */
        int INITIALIZING = 1;

        /** OnResume() is called after StartActivity(). */
        int STARTED = 3;
    }

    /**
     * Returns the singleton instance, lazily creating one if needed.
     *
     * @return The singleton instance.
     */
    public static DownloadActivityLauncher getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            sInstance = new DownloadActivityLauncher();
        }
        return sInstance;
    }

    /**
     * Launches the download activity on phones.
     *
     * @param activity The current activity is available.
     * @param otrProfileID The {@link OTRProfileID} to determine whether download home should be
     *     opened in incognito mode. Only used when no valid current or recent tab presents.
     * @param showPrefetchedContent Whether the manager should start with prefetched content section
     *     expanded.
     */
    public void showDownloadActivity(
            @Nullable Activity activity,
            @Nullable OTRProfileID otrProfileID,
            boolean showPrefetchedContent) {
        Context appContext = ContextUtils.getApplicationContext();

        Intent intent = new Intent();
        intent.setClass(appContext, DownloadActivity.class);
        intent.putExtra(EXTRA_SHOW_PREFETCHED_CONTENT, showPrefetchedContent);
        if (otrProfileID != null) {
            intent.putExtra(
                    DownloadUtils.EXTRA_OTR_PROFILE_ID, OTRProfileID.serialize(otrProfileID));
        }

        if (activity == null) {
            // Stands alone in its own task.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            appContext.startActivity(intent);
        } else {
            // Sits on top of another Activity.
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            activity.startActivity(intent);
        }
        // Start tracking the DownloadActivity status.
        if (mActivityStatus == DownloadActivityStatus.NOT_CREATED) {
            mActivityStatus = DownloadActivityStatus.INITIALIZING;
            ApplicationStatus.registerStateListenerForAllActivities(this);
        }
    }

    /**
     * @return Whether or not the prefetched content section should be expanded on launch of the
     *     DownloadActivity.
     */
    public static boolean shouldShowPrefetchContent(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_SHOW_PREFETCHED_CONTENT, false);
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (!(activity instanceof DownloadActivity)) return;
        if (newState == ActivityState.RESUMED) {
            mActivityStatus = DownloadActivityStatus.STARTED;
            for (Callback<Activity> callback : mActivityCallbacks) {
                callback.onResult(activity);
            }
            mActivityCallbacks.clear();
        } else if (newState == ActivityState.DESTROYED) {
            mActivityStatus = DownloadActivityStatus.NOT_CREATED;
            ApplicationStatus.unregisterActivityStateListener(this);
        }
    }

    public void getActivityForOpenDialog(Callback<Activity> callback) {
        if (mActivityStatus != DownloadActivityStatus.INITIALIZING) {
            Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
            if (activity instanceof ModalDialogManagerHolder) {
                callback.onResult(activity);
            } else {
                mActivityCallbacks.add(callback);
            }
        } else {
            mActivityCallbacks.add(callback);
        }
    }
}
