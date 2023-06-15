// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for partner customizations and loading.
 */
class PartnerCustomizationsUma {
    private static final String TAG = "PartnerCustUma";

    /**
     * Tracks which delegate is first used for any purpose.
     * Trying to switch to another is logged.
     */
    private static @CustomizationProviderDelegateType int sWhichDelegate =
            CustomizationProviderDelegateType.NONE_VALID;

    private long mAsyncStartTime;

    /**
     * Records whether the async task ran to completion before the initial Tab was created.
     * A value of {@code null} indicates that we have not yet created the initial Tab.
     */
    @Nullable
    private Boolean mWasInitializedBeforeCreateInitialTab;
    private long mCreateInitialTabTime;

    /**
     * Called when Chrome is about to create an initial tab.
     * Logs that by the time {@link PartnerBrowserCustomizations#initializeAsync} is called, whether
     * {@link PartnerBrowserCustomizations#isInitialized}. For cases that's not initialized - due to
     * timeout - we are at risk of creating the initial tab with homepage different than the partner
     * provided homepage.
     * @param isInitialized Whether initialization completed vs timed out.
     */
    void onCreateInitialTab(boolean isInitialized) {
        RecordHistogram.recordBooleanHistogram(
                "Android.PartnerCustomizationInitializedBeforeInitialTab", isInitialized);

        // Handle the rare case where more than one ChromeTabbedActivity is creating an initial
        // tab before the Partner Customizations initialization has completed. In this case we
        // ignore everything associated with Tab creation by the second activity altogether by
        // exiting here.
        if (mWasInitializedBeforeCreateInitialTab != null) return;

        mWasInitializedBeforeCreateInitialTab = isInitialized;
        mCreateInitialTabTime = SystemClock.elapsedRealtime();

        // TODO(donnd): log similar data when no Homepage has ever been determined - Chrome first
        // launch.
    }

    /**
     * Logs whether we failed to create an initial tab due to the app finishing or being destroyed.
     * @param isActivityFinishingOrDestroyed Whether the Activity is going away.
     */
    static void logActivityFinishingOrDestroyed(boolean isActivityFinishingOrDestroyed) {
        RecordHistogram.recordBooleanHistogram(
                "Android.PartnerCustomization.ActivityFinishingOrDestroyed",
                isActivityFinishingOrDestroyed);
    }

    /**
     * Constants used to identify the delegate being used.
     * These correspond to PartnerCustomizationProviderDelegate in enums.xml.
     * These values are recorded as histogram values. Entries should not be renumbered and numeric
     * values should never be reused.
     */
    @IntDef({CustomizationProviderDelegateType.NONE_VALID,
            CustomizationProviderDelegateType.PHENOTYPE,
            CustomizationProviderDelegateType.G_SERVICE,
            CustomizationProviderDelegateType.PRELOAD_APK,
            CustomizationProviderDelegateType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface CustomizationProviderDelegateType {
        int NONE_VALID = 0;
        int PHENOTYPE = 1;
        int G_SERVICE = 2;
        int PRELOAD_APK = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Records which delegate will be used to provide the home page URL that is needed for a custom
     * provider's home page. Should be called at startup. Called from Downstream, or unused.
     * @param whichDelegate The {@link CustomizationProviderDelegateType} being used.
     */
    public static void logPartnerCustomizationDelegate(
            @CustomizationProviderDelegateType int whichDelegate) {
        RecordHistogram.recordEnumeratedHistogram("Android.PartnerHomepageCustomization.Delegate2",
                whichDelegate, CustomizationProviderDelegateType.NUM_ENTRIES);

        Log.i(TAG, "Partner Customization delegate: %s.", whichDelegate);
        sWhichDelegate = whichDelegate;
    }

    /**
     * What kind of customization is actually used.
     * These correspond to PartnerCustomizationUsage in enums.xml.
     * These values are recorded as histogram values. Entries should not be renumbered and numeric
     * values should never be reused.
     */
    @IntDef({CustomizationUsage.HOMEPAGE, CustomizationUsage.BOOKMARKS,
            CustomizationUsage.INCOGNITO, CustomizationUsage.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface CustomizationUsage {
        int HOMEPAGE = 0;
        int BOOKMARKS = 1;
        int INCOGNITO = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * Records which customization usage is being invoked, at the time it is requested. Called from
     * Downstream, or unused.
     * @param usage The {@link CustomizationUsage} used, e.g. home page vs incognito.
     */
    public static void logPartnerCustomizationUsage(@CustomizationUsage int usage) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PartnerCustomization.Usage", usage, CustomizationUsage.NUM_ENTRIES);
    }

    static void logPartnerBrowserCustomizationInitDuration(long startTime, long endTime) {
        // Legacy Histogram, do not modify name or whether written or not.
        assert startTime > 0;
        assert endTime > 0;
        long duration = endTime - startTime;
        assert duration >= 0;
        RecordHistogram.recordTimesHistogram(
                "Android.PartnerBrowserCustomizationInitDuration", duration);
    }

    /**
     * Logs the duration of initialization of the async task, including notifying callbacks.
     */
    static void logPartnerBrowserCustomizationInitDurationWithCallbacks(
            long startTime, long endTime) {
        // Legacy Histogram, do not modify name or whether written or not.
        assert startTime > 0;
        assert endTime > 0;
        long duration = endTime - startTime;
        assert duration >= 0;
        RecordHistogram.recordTimesHistogram(
                "Android.PartnerBrowserCustomizationInitDuration.WithCallbacks", duration);
    }

    /**
     * Describes why a particular Customization Delegate could not be used.
     * These correspond to PartnerDelegateUnusedReason in enums.xml.
     * These values are recorded as histogram values. Entries should not be renumbered and numeric
     * values should never be reused.
     */
    @IntDef({
            DelegateUnusedReason.PHENOTYPE_BEFORE_PIE,
            DelegateUnusedReason.PHENOTYPE_FLAG_CANT_COMMIT,
            DelegateUnusedReason.PHENOTYPE_FLAG_CONFIG_EMPTY,
            DelegateUnusedReason.GSERVICES_GET_TIMESTAMP_EXCEPTION,
            DelegateUnusedReason.GSERVICES_TIMESTAMP_MISSING,
            DelegateUnusedReason.PRELOAD_APK_CANNOT_RESOLVE_PROVIDER,
            DelegateUnusedReason.PRELOAD_APK_NOT_SYSTEM_PROVIDER,
            DelegateUnusedReason.NUM_ENTRIES,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DelegateUnusedReason {
        int PHENOTYPE_BEFORE_PIE = 0;
        int PHENOTYPE_FLAG_CANT_COMMIT = 1;
        int PHENOTYPE_FLAG_CONFIG_EMPTY = 2;
        int GSERVICES_GET_TIMESTAMP_EXCEPTION = 3;
        int GSERVICES_TIMESTAMP_MISSING = 4;
        int PRELOAD_APK_CANNOT_RESOLVE_PROVIDER = 5;
        int PRELOAD_APK_NOT_SYSTEM_PROVIDER = 6;

        int NUM_ENTRIES = 7;
    }

    /**
     * Logs a reason that a Partner Customization Delegate was not used.
     * May be recorded up to 3 times when all 3 are delegates are skipped.
     * Called from Downstream, or unused.
     * @param reasonForDelegateNotUsed The delegate and reason it's not usable.
     */
    static void logDelegateUnusedReason(@DelegateUnusedReason int reasonForDelegateNotUsed) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PartnerCustomization.DelegateUnusedReason", reasonForDelegateNotUsed,
                DelegateUnusedReason.NUM_ENTRIES);
    }

    /**
     * Logs that we just tried to create a customization delegate, if that failed, and the duration.
     * Called from Downstream, or unused.
     * @param delegate The kind of delegate we tried to create.
     * @param startTime The elapsed real time when we started to create it.
     * @param didTryCreateSucceed Whether the create operation did succeed.
     */
    static void logDelegateTryCreateDuration(@CustomizationProviderDelegateType int delegate,
            long startTime, boolean didTryCreateSucceed) {
        final long createdTime = SystemClock.elapsedRealtime();
        logDelegateTryCreateDuration(delegate, startTime, createdTime, didTryCreateSucceed);
    }

    @VisibleForTesting
    static void logDelegateTryCreateDuration(@CustomizationProviderDelegateType int delegate,
            long startTime, long endTime, boolean didTryCreateSucceed) {
        long duration = endTime - startTime;
        String durationHistogramName;
        if (didTryCreateSucceed) {
            durationHistogramName = "Android.PartnerCustomization.TrySucceededDuration.";
        } else {
            durationHistogramName = "Android.PartnerCustomization.TryFailedDuration.";
        }
        RecordHistogram.recordTimesHistogram(
                durationHistogramName + delegateName(delegate), duration);
    }

    /**
     * Called when the partner customization Async Init background task is started.
     */
    void logAsyncInitStarted() {
        assert mAsyncStartTime == 0;
        mAsyncStartTime = SystemClock.elapsedRealtime();
        sWhichDelegate = CustomizationProviderDelegateType.NONE_VALID;
    }

    /**
     * Called when the partner customization Async Init background task completes.
     */
    void logAsyncInitCompleted() {
        @TaskCompletion
        int taskCompletion = TaskCompletion.NONE_VALID;
        if (mAsyncStartTime != 0) {
            final long completedTime = SystemClock.elapsedRealtime();
            logLoadDuration(mAsyncStartTime, completedTime, sWhichDelegate);
            // Check if we've already tried to create an initial tab and record timing for the miss.
            if (mWasInitializedBeforeCreateInitialTab == null) {
                taskCompletion = TaskCompletion.COMPLETED_IN_TIME;
            } else if (!mWasInitializedBeforeCreateInitialTab) {
                taskCompletion = TaskCompletion.COMPLETED_TOO_LATE;
                RecordHistogram.recordTimesHistogram(
                        "Android.PartnerCustomization.DurationNeededForAsyncCompletion",
                        completedTime - mCreateInitialTabTime);
            }
        }
        recordTaskCompletion(taskCompletion);
        sWhichDelegate = CustomizationProviderDelegateType.NONE_VALID;
    }

    /**
     * Called whenever the partner customization Async Init background task is cancelled.
     */
    void logAsyncInitCancelled() {
        recordTaskCompletion(TaskCompletion.CANCELLED);
    }

    /**
     * Called if the partner customization Async Init background task throws an exception.
     */
    void logAsyncInitException() {
        recordTaskCompletion(TaskCompletion.EXCEPTION);
    }

    /** The different outcomes for the Async Task completion. */
    @IntDef({
            TaskCompletion.NONE_VALID,
            TaskCompletion.COMPLETED_IN_TIME,
            TaskCompletion.COMPLETED_TOO_LATE,
            TaskCompletion.CANCELLED,
            TaskCompletion.EXCEPTION,
            TaskCompletion.NUM_ENTRIES,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TaskCompletion {
        int NONE_VALID = 0;
        int COMPLETED_IN_TIME = 1;
        int COMPLETED_TOO_LATE = 2;
        int CANCELLED = 3;
        int EXCEPTION = 4;

        int NUM_ENTRIES = 5;
    }

    /**
     * Logs how the async task completed.
     */
    private static void recordTaskCompletion(@TaskCompletion int taskCompletionEnum) {
        RecordHistogram.recordEnumeratedHistogram("Android.PartnerCustomization.TaskCompletion",
                taskCompletionEnum, TaskCompletion.NUM_ENTRIES);
    }

    /**
     * Logs a duration for loading data from the {@link CustomizationProviderDelegateType} when
     * successful.
     */
    private static void logLoadDuration(long startTime, long completedTime,
            @CustomizationProviderDelegateType int whichDelegate) {
        assert startTime > 0;
        assert completedTime > 0;
        long duration = completedTime - startTime;
        assert duration >= 0;
        RecordHistogram.recordTimesHistogram(
                "Android.PartnerCustomization.LoadDuration." + delegateName(whichDelegate),
                duration);
    }

    /** @return the variant name for the given delegate for use in variant histograms. */
    private static String delegateName(@CustomizationProviderDelegateType int delegate) {
        switch (delegate) {
            case CustomizationProviderDelegateType.G_SERVICE:
                return "GService";
            case CustomizationProviderDelegateType.PHENOTYPE:
                return "Phenotype";
            case CustomizationProviderDelegateType.PRELOAD_APK:
                return "PreloadApk";
            default:
                return "None";
        }
    }
}
