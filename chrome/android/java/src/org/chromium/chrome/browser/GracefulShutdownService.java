// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.SplitCompatService;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;

/** Service that keeps Chrome process alive during window shutdown to allow graceful teardown. */
@NullMarked
public class GracefulShutdownService extends SplitCompatService {
    private static final String TAG = "GracefulShutdown";

    // LINT.IfChange(GracefulShutdownStatus)
    @IntDef({
        Status.LAUNCH_ATTEMPTED,
        Status.LAUNCH_FAILED,
        Status.STARTED,
        Status.START_FOREGROUND_FAILED,
        Status.TIMED_OUT
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface Status {
        int LAUNCH_ATTEMPTED = 0;
        int LAUNCH_FAILED = 1;
        int STARTED = 2;
        int START_FOREGROUND_FAILED = 3;
        int TIMED_OUT = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:GracefulShutdownStatus)

    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.GracefulShutdownServiceImpl";

    public GracefulShutdownService() {
        super(sImplClassName);
    }

    /** Checks if the currently dying activity is the last remaining active Activity. */
    public static boolean isLastActivityDying() {
        if (!ApplicationStatus.isInitialized()) {
            return false;
        }
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        if (activities == null || activities.isEmpty()) {
            return false;
        }
        int activeCount = 0;
        int finishingCount = 0;
        for (Activity activity : activities) {
            if (activity.isFinishing()) {
                finishingCount++;
            } else if (!activity.isDestroyed()) {
                activeCount++;
            }
        }
        boolean result = activeCount == 0 && finishingCount > 0;
        Log.i(
                TAG,
                "isLastActivityDying result: "
                        + result
                        + " (active="
                        + activeCount
                        + ", finishing="
                        + finishingCount
                        + ")");
        return result;
    }

    /** Starts the GracefulShutdownService ONLY if the last open activity is dying. */
    public static void maybeStartGracefulShutdown(Context context) {
        if (!ChromeFeatureList.sTabAndroidGracefulShutdown.isEnabled()) {
            return;
        }
        if (!isLastActivityDying()) {
            return;
        }
        Log.i(TAG, "Starting GracefulShutdownService.");
        try {
            Intent intent = new Intent(context, GracefulShutdownService.class);
            context.startForegroundService(intent);
            recordStatus(Status.LAUNCH_ATTEMPTED);
        } catch (Throwable e) {
            recordStatus(Status.LAUNCH_FAILED);
            Log.e(TAG, "Failed to start GracefulShutdownService", e);
        }
    }

    public static void recordStatus(@Status int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Tab.Android.GracefulShutdownStatus", status, Status.NUM_ENTRIES);
    }
}
