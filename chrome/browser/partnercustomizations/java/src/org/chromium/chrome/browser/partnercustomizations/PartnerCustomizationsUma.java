// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_CORRECTLY;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_INCORRECTLY;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_UNKNOWN;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.OTHER_CUSTOM_HOMEPAGE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.PARTNER_CUSTOM_HOMEPAGE;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for partner customizations and loading.
 * Each of these UMA instances correspond to a {@link PartnerBrowserCustomizations#initializeAsync}
 * execution. If createInitialTab is called, it's associated with whichever UMA started most
 * recently. Then that UMA logs the interaction with customization and won't log again in any future
 * instance.
 */
class PartnerCustomizationsUma {
    private static final String TAG = "PartnerCustUma";

    /**
     * Whether any {@link PartnerBrowserCustomizations#initializeAsync} has finished its run, either
     * successfully or not. They can run serially, and we only associate the createInitialTab
     * outcome with the first run.
     */
    private static boolean sIsAnyInitializeAsyncFinalized;

    /** Tracks which delegate is first used for any purpose. */
    private static @CustomizationProviderDelegateType int sWhichDelegate =
            CustomizationProviderDelegateType.NONE_VALID;

    /** Whether we have logged the outcome of creating an initialTab during customization. */
    private static boolean sInitialTabOutcomeHasBeenLogged;

    /** When the async customization task started. */
    private long mAsyncCustomizationStartTime;
    /** Whether the homepage was already cached at the beginning of the async customization task. */
    private final boolean mWasHomepageCached;
    /** Whether the process of customization changed the Homepage. */
    private boolean mWasHomepageUriChanged;
    /** Whether the async customization task completed successfully (not just finalized). */
    private boolean mDidCustomizationCompleteSuccessfully;

    /**
     * Records whether an initial Tab was created after the customization task ran to completion.
     * A value of {@code null} indicates that we did not yet create the initial Tab.
     */
    private @Nullable Boolean mDidCreateInitialTabAfterCustomization;
    private @Nullable String mHomepageUrlCreated;
    private boolean mIsOverviewPageOrStartSurface;
    private long mCreateInitialTabTime;

    /** Supplies access to HomepageManager to characterize homepages. */
    private @Nullable Supplier<HomepageCharacterizationHelper> mHomepageCharacterizationHelper;

    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * Logs to UMA for {@link PartnerBrowserCustomizations}.
     * Tracks - and logs to UMA - significant events that occur on any single run of {@link
     * PartnerBrowserCustomizations#initializeAsync}. Also logs the outcome of creating an initial
     * tab at Chrome startup and how that relates to the Homepage, and Partner Customization in
     * general.
     */
    PartnerCustomizationsUma() {
        sWhichDelegate = CustomizationProviderDelegateType.NONE_VALID;
        mWasHomepageCached = isHomepageCached();
    }

    /**
     * Called when Chrome is about to create an initial tab.
     * Logs that by the time {@link PartnerBrowserCustomizations#initializeAsync} is called, whether
     * {@link PartnerBrowserCustomizations#isInitialized}. For cases that's not initialized - due to
     * timeout - we are at risk of creating the initial tab with homepage different than the partner
     * provided homepage.
     * @param isInitialized Whether initialization completed vs timed out.
     * @param homepageUrlCreated The URL of the initial Tab that was created or {@code null} if
     *         something other than a Homepage was used.
     * @param createInitialTabTime The timestamp when we started to create an initial tab.
     * @param isOverviewPageOrStartSurface indicates that there was no created Homepage because some
     *         kind of overview page or Start Surface was presented in place of the initial Tab.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} to use to wait for
     *        native initialization.
     * @param homepageCharacterizationHelper A supplier for Homepage characterization needs in
     *        {@link PartnerCustomizationsUma}.
     */
    void onCreateInitialTab(boolean isInitialized, @Nullable String homepageUrlCreated,
            long createInitialTabTime, boolean isOverviewPageOrStartSurface,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<HomepageCharacterizationHelper> homepageCharacterizationHelper) {
        assert (isOverviewPageOrStartSurface || homepageUrlCreated != null)
            : "Null created Homepage unexpected unless Overview Page!";
        RecordHistogram.recordBooleanHistogram(
                "Android.PartnerCustomizationInitializedBeforeInitialTab", isInitialized);
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mHomepageCharacterizationHelper = homepageCharacterizationHelper;

        // Handle the rare case where more than one ChromeTabbedActivity is creating an initial
        // tab before the Partner Customizations initialization has completed. In this case we
        // ignore everything associated with Tab creation by the second activity altogether by
        // exiting here.
        if (mDidCreateInitialTabAfterCustomization != null) {
            Log.w(TAG, "Multiple initial Tabs being created, e.g. multi-instance.");
            return;
        }

        mIsOverviewPageOrStartSurface = isOverviewPageOrStartSurface;
        mDidCreateInitialTabAfterCustomization = isInitialized;
        mCreateInitialTabTime = createInitialTabTime;
        mHomepageUrlCreated = homepageUrlCreated;
        tryLogInitialTabCustomizationOutcome();
    }

    /**
     * Constants used to categorize what kind of Homepage was initially shown at Chrome's startup.
     * These correspond to PartnerCustomizationsHomepage in enums.xml.
     * These values are recorded as histogram values. Entries should not be renumbered and numeric
     * values should never be reused.
     */
    @IntDef({
            PartnerCustomizationsHomepageEnum.NTP_UNKNOWN,
            PartnerCustomizationsHomepageEnum.NTP_INCORRECTLY,
            PartnerCustomizationsHomepageEnum.NTP_CORRECTLY,
            PartnerCustomizationsHomepageEnum.PARTNER_CUSTOM_HOMEPAGE,
            PartnerCustomizationsHomepageEnum.OTHER_CUSTOM_HOMEPAGE,

            PartnerCustomizationsHomepageEnum.NUM_ENTRIES,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface PartnerCustomizationsHomepageEnum {
        /**
         * Initial tab is NTP but it's unknown whether this is correct or not because loading
         * partner customizations did not complete.
         */
        int NTP_UNKNOWN = 0;

        /** Initial tab is NTP but should have been a non-ntp partner homepage. */
        int NTP_INCORRECTLY = 1;

        /**
         * Initial tab is NTP and should have been. Partner provided NTP, user setting to use NTP,
         * or no partner homepage was provided so we default to NTP.
         */
        int NTP_CORRECTLY = 2;

        /**
         * Initial tab matches partner provided homepage either from cache or set by customization.
         */
        int PARTNER_CUSTOM_HOMEPAGE = 3;

        /** Initial tab is some custom homepage other than the NTP, or the Partner homepage. */
        int OTHER_CUSTOM_HOMEPAGE = 4;

        int NUM_ENTRIES = 5;
    }

    /**
     * Tries to complete the Homepage customization.
     * Called when the async task finishes, and also when the initial tab creation is happening.
     * When both calls have been made we can log the final outcomes.
     */
    private void tryLogInitialTabCustomizationOutcome() {
        if (!sIsAnyInitializeAsyncFinalized || mDidCreateInitialTabAfterCustomization == null
                || sInitialTabOutcomeHasBeenLogged) {
            return;
        }
        sInitialTabOutcomeHasBeenLogged = true;
        logInitialTabCustomizationOutcome(mActivityLifecycleDispatcher);
        mActivityLifecycleDispatcher = null;
    }

    /**
     * Logs the outcome of creating an initial tab relative to Partner customization.
     * @param activityLifecycleDispatcher A lifecycle dispatcher used to delay any execution that
     *                                    might be risky until after native initialization.
     */
    @VisibleForTesting
    void logInitialTabCustomizationOutcome(
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        // Snapshot static members in case a new async task starts up while we're delayed.
        final @CustomizationProviderDelegateType int whichDelegate = sWhichDelegate;

        assert activityLifecycleDispatcher != null;
        // To be safe, we delay until after native initialization and flag guard.
        // Any operations that are risky or not OK before native initialization should be pushed
        // down into this section.
        onFinishNativeInitializationOrEnabled(activityLifecycleDispatcher, () -> {
            assert mDidCreateInitialTabAfterCustomization != null;

            boolean isInitialTabNtpOrOverview =
                    mHomepageCharacterizationHelper.get().isUrlNtp(mHomepageUrlCreated)
                    || mIsOverviewPageOrStartSurface;
            boolean isHomepagePartner = mHomepageCharacterizationHelper.get().isPartner();
            boolean isHomepageNtp = mHomepageCharacterizationHelper.get().isNtp();
            // We can be certain that our Homepage characterization is correct if the Homepage URI
            // changed, which might have happened before it ran to completion.
            boolean isCharacterizationCertain =
                    mDidCustomizationCompleteSuccessfully || mWasHomepageUriChanged;

            logInitialTabCustomizationOutcomeDelayed(whichDelegate, isInitialTabNtpOrOverview,
                    isCharacterizationCertain, isHomepagePartner, isHomepageNtp,
                    mWasHomepageCached);
        });
    }

    /**
     * Logs the outcome of creating an initial tab relative to Partner customization.
     * @param whichDelegate Which delegate did the customization.
     * @param isInitialTabNtpOrOverview Whether the initial Tab created was the system NTP or an
     *         Overview page like the Start Surface vs a custom Homepage.
     * @param isCharacterizationCertain Whether the homepageCharacterization is certain based on a
     *        successful completion of the async task or because the Homepage was changed.
     * @param isHomepagePartner Whether the current Homepage is a Partner Homepage or NTP.
     * @param isHomepageNtp Whether the current Homepage is any kind of NTP (new tab page).
     * @param wasHomepageCached Whether a homepage was already recorded in the cache before the
     *         customization started.
     */
    @VisibleForTesting
    void logInitialTabCustomizationOutcomeDelayed(
            @CustomizationProviderDelegateType int whichDelegate, boolean isInitialTabNtpOrOverview,
            boolean isCharacterizationCertain, boolean isHomepagePartner, boolean isHomepageNtp,
            boolean wasHomepageCached) {
        @PartnerCustomizationsHomepageEnum
        int partnerCustomizationHomepageEnum;

        if (isInitialTabNtpOrOverview) {
            if (!isCharacterizationCertain) {
                partnerCustomizationHomepageEnum = NTP_UNKNOWN;
            } else {
                partnerCustomizationHomepageEnum = isHomepageNtp ? NTP_CORRECTLY : NTP_INCORRECTLY;
            }
        } else {
            partnerCustomizationHomepageEnum =
                    isHomepagePartner ? PARTNER_CUSTOM_HOMEPAGE : OTHER_CUSTOM_HOMEPAGE;
        }
        logPartnerCustomizationHomepage(
                partnerCustomizationHomepageEnum, whichDelegate, wasHomepageCached);
    }

    /**
     * Whether the system has a cached Homepage. This could be a custom homepage or the system NTP.
     */
    private boolean isHomepageCached() {
        // TODO(https://crbug.com/1456533): merge into HomepageManager.
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        return (sharedPreferencesManager.readString(
                        ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, null)
                       != null)
                || (sharedPreferencesManager.readString(
                            ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI, null)
                        != null);
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
        /**
         * No delegate was usable. Likely due to the default delegate {@link
         * CustomizationProviderDelegateUpstreamImpl#isValid} returned false. See also
         * DelegateUnusedReason cases for PreloadApk.
         */
        int NONE_VALID = 0;
        int PHENOTYPE = 1;
        int G_SERVICE = 2;
        int PRELOAD_APK = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Records which delegate will be used to provide the home page URL that is needed for a custom
     * provider's home page. Should be called at startup.
     * Called from Downstream, or unused.
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
        logAsyncInitStarted(SystemClock.elapsedRealtime());
    }

    @VisibleForTesting
    void logAsyncInitStarted(long initializeAsyncStartTime) {
        assert mAsyncCustomizationStartTime == 0;
        mAsyncCustomizationStartTime = initializeAsyncStartTime;
        sWhichDelegate = CustomizationProviderDelegateType.NONE_VALID;
    }

    /**
     * Called when the partner customization Async Init background task completes successfully.
     */
    void logAsyncInitCompleted() {
        logAsyncInitCompleted(SystemClock.elapsedRealtime());
    }

    @VisibleForTesting
    void logAsyncInitCompleted(long initCompletedTime) {
        mDidCustomizationCompleteSuccessfully = true;
        @TaskCompletion
        int taskCompletion = TaskCompletion.NONE_VALID;
        if (mAsyncCustomizationStartTime != 0) {
            logLoadDuration(mAsyncCustomizationStartTime, initCompletedTime, sWhichDelegate);
            // Check if we've already tried to create an initial tab.
            if (mDidCreateInitialTabAfterCustomization == null
                    || mDidCreateInitialTabAfterCustomization) {
                taskCompletion = TaskCompletion.COMPLETED_IN_TIME;
            } else {
                taskCompletion = TaskCompletion.COMPLETED_TOO_LATE;
            }
        } else {
            // Never called #logAsyncInitStarted, probably due to immediate cancellation and/or the
            // task was put into the executor but never start.
            taskCompletion = TaskCompletion.TASK_SKIPPED;
        }
        logTaskCompletion(taskCompletion, sWhichDelegate, mWasHomepageCached);

        // Record how much longer we would need to wait in createInitialTab in order to have this
        // async completion done in time to pick up the customization.
        if (mDidCreateInitialTabAfterCustomization != null
                && !mDidCreateInitialTabAfterCustomization) {
            logDurationNeededForAsyncCompletion(
                    mCreateInitialTabTime, initCompletedTime, sWhichDelegate, mWasHomepageCached);
        }
    }

    /**
     * Called whenever the {@link PartnerBrowserCustomizations#initializeAsync} background task is
     * cancelled.
     */
    void logAsyncInitCancelled() {
        logTaskCompletion(TaskCompletion.CANCELLED, sWhichDelegate, mWasHomepageCached);
    }

    /**
     * Called if the {@link PartnerBrowserCustomizations#initializeAsync} task throws an exception.
     */
    void logAsyncInitException() {
        logTaskCompletion(TaskCompletion.EXCEPTION, sWhichDelegate, mWasHomepageCached);
    }

    /**
     * Called when the {@link PartnerBrowserCustomizations#initializeAsync} background task finishes
     * under any condition.
     */
    void logAsyncInitFinalized(long startTime, long finalizedTime, boolean wasHomepageUriChanged) {
        logPartnerBrowserCustomizationInitDurationWithCallbacks(startTime, finalizedTime);

        sIsAnyInitializeAsyncFinalized = true;
        mWasHomepageUriChanged = wasHomepageUriChanged;
        tryLogInitialTabCustomizationOutcome();
    }

    /**
     * Logs the outcome of Homepage customization. This indicates what the user saw: NTP vs'
     * some other page and whether that was the correct page to show after customization completes.
     * @param partnerCustomizationHomepageEnum The code for what kind of page was shown.
     * @param whichDelegate Which delegate was doing the customization.
     * @param wasHomepageCached Whether the homepage was cached, which effectively tells us that
     *                          this was not Chrome's first ever launch.
     */
    void logPartnerCustomizationHomepage(
            @PartnerCustomizationsHomepageEnum int partnerCustomizationHomepageEnum,
            @CustomizationProviderDelegateType int whichDelegate, boolean wasHomepageCached) {
        String delegateName = delegateName(whichDelegate);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PartnerCustomization.HomepageCustomizationOutcome",
                partnerCustomizationHomepageEnum, PartnerCustomizationsHomepageEnum.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PartnerCustomization.HomepageCustomizationOutcome." + delegateName,
                partnerCustomizationHomepageEnum, PartnerCustomizationsHomepageEnum.NUM_ENTRIES);
        if (!wasHomepageCached) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.PartnerCustomization.HomepageCustomizationOutcomeNotCached."
                            + delegateName,
                    partnerCustomizationHomepageEnum,
                    PartnerCustomizationsHomepageEnum.NUM_ENTRIES);
        }
    }

    /** The different outcomes for the Async Task completion. */
    @IntDef({
            TaskCompletion.NONE_VALID,
            TaskCompletion.COMPLETED_IN_TIME,
            TaskCompletion.COMPLETED_TOO_LATE,
            TaskCompletion.CANCELLED,
            TaskCompletion.EXCEPTION,
            TaskCompletion.TASK_SKIPPED,

            TaskCompletion.NUM_ENTRIES,
    })
    @VisibleForTesting
    @Retention(RetentionPolicy.SOURCE)
    @interface TaskCompletion {
        int NONE_VALID = 0;
        int COMPLETED_IN_TIME = 1;
        int COMPLETED_TOO_LATE = 2;
        int CANCELLED = 3;
        int EXCEPTION = 4;
        int TASK_SKIPPED = 5;

        int NUM_ENTRIES = 6;
    }

    /**
     * Logs how the async task completed.
     */
    private static void logTaskCompletion(@TaskCompletion int taskCompletionEnum,
            @CustomizationProviderDelegateType int whichDelegate, boolean wasHomepageCached) {
        RecordHistogram.recordEnumeratedHistogram("Android.PartnerCustomization.TaskCompletion",
                taskCompletionEnum, TaskCompletion.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PartnerCustomization.TaskCompletion." + delegateName(whichDelegate),
                taskCompletionEnum, TaskCompletion.NUM_ENTRIES);
        if (!wasHomepageCached) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.PartnerCustomization.TaskCompletionNotCached."
                            + delegateName(whichDelegate),
                    taskCompletionEnum, TaskCompletion.NUM_ENTRIES);
        }
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
    @VisibleForTesting
    static String delegateName(@CustomizationProviderDelegateType int delegate) {
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

    /**
     * Records how much longer we would need to wait in createInitialTab to have this async task
     * completion done in time to pick up the customization.
     * @param createInitialTabTime The time when the initial Tab was created.
     * @param completedTime The time when the partner customization completed.
     * @param whichDelegate Which delegate was doing the customization.
     * @param wasHomepageCached Whether this is effectively Chrome's first launch.
     */
    private static void logDurationNeededForAsyncCompletion(long createInitialTabTime,
            long completedTime, @CustomizationProviderDelegateType int whichDelegate,
            boolean wasHomepageCached) {
        assert createInitialTabTime > 0;
        assert completedTime > 0;
        long duration = completedTime - createInitialTabTime;
        assert duration >= 0 : String.format("Completed time %s must be >= start time %s",
                                       completedTime, createInitialTabTime);
        RecordHistogram.recordTimesHistogram(
                "Android.PartnerCustomization.DurationNeededForAsyncCompletion", duration);
        RecordHistogram.recordTimesHistogram(
                "Android.PartnerCustomization.DurationNeededForAsyncCompletion."
                        + delegateName(whichDelegate),
                duration);
        if (!wasHomepageCached) {
            RecordHistogram.recordTimesHistogram(
                    "Android.PartnerCustomization.DurationNeededForAsyncCompletionNotCached",
                    duration);
            RecordHistogram.recordTimesHistogram(
                    "Android.PartnerCustomization.DurationNeededForAsyncCompletionNotCached."
                            + delegateName(whichDelegate),
                    duration);
        }
    }

    /**
     * Determines whether UMA logging of Partner Customization - the functionality of this class -
     * is enabled or not.
     */
    @VisibleForTesting
    static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA);
    }

    /**
     * @return whether the system was ready to execute or skip the given {@link Runnable}. Returns
     *         {@code false} when Features are not yet initialized or this UMA Feature is disabled.
     */
    private static boolean didExecute(Runnable closure) {
        if (!FeatureList.isInitialized()) return false;

        if (isEnabled()) {
            closure.run();
        }
        return true;
    }

    /**
     * Delays the execution of the given {@link Runnable} until enabled or native is initialized.
     * Care should be taken when crafting the chore to be executed such that any timely information
     * is captured outside of the chore in a final local member, and implicitly passed into the
     * method for subsequent processing. An example is grabbing an end time for some process from
     * the clock (see usages in this file).
     * If this Feature is not enabled then calling this is a noop, and the chore will not be
     * executed.
     */
    @VisibleForTesting
    void onFinishNativeInitializationOrEnabled(
            ActivityLifecycleDispatcher activityLifecycleDispatcher, Runnable choreWhenEnabled) {
        if (!didExecute(choreWhenEnabled)) {
            NativeInitObserver nativeInitObserver = new NativeInitObserver() {
                @Override
                public void onFinishNativeInitialization() {
                    activityLifecycleDispatcher.unregister(this);
                    if (isEnabled()) {
                        choreWhenEnabled.run();
                    }
                }
            };
            activityLifecycleDispatcher.register(nativeInitObserver);
        }
    }

    @VisibleForTesting
    static void resetStaticsForTesting() {
        sWhichDelegate = CustomizationProviderDelegateType.NONE_VALID;
        sInitialTabOutcomeHasBeenLogged = false;
        sIsAnyInitializeAsyncFinalized = false;
    }
}
