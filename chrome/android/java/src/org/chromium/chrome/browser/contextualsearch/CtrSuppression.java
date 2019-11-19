// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Provides a ContextualSearchHeuristic for CTR Recording, logging, and eventually suppression.
 * Records impressions and CTR when the Bar is dismissed.
 * TODO(donnd): add suppression logic.
 * Logs "impressions" and "CTR" per user in UMA for the previous week and 28-day period.
 * An impression is a view of our UX (the Bar) and CTR is the "click-through rate" (user opens of
 * the Bar).
 * This class also implements the device-based native integer storage required by the
 * native {@code CtrAggregator} class.
 */
public class CtrSuppression extends ContextualSearchHeuristic {
    private long mNativePointer;

    private static Integer sCurrentWeekNumberCache;

    private final SharedPreferencesManager mPreferenceManager;

    /**
     * Constructs an object that tracks impressions and clicks per user to produce CTR and
     * impression metrics.
     */
    CtrSuppression() {
        mPreferenceManager = SharedPreferencesManager.getInstance();

        // This needs to be done last in this constructor because the native code may call
        // into this object.
        mNativePointer = CtrSuppressionJni.get().init(CtrSuppression.this);
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.
     */
    public void destroy() {
        if (mNativePointer != 0L) {
            CtrSuppressionJni.get().destroy(mNativePointer, CtrSuppression.this);
        }
    }

    // ============================================================================================
    // ContextualSearchHeuristic overrides.
    // ============================================================================================

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return false;
    }

    @Override
    protected void logConditionState() {
        // Since the CTR for previous time periods never changes, we only need to write to the UMA
        // log when we may have moved to a new week, or we have new persistent data.
        // We cache the current week in persistent storage so we can tell when it changes.
        boolean didWeekChange = didWeekChange(
                CtrSuppressionJni.get().getCurrentWeekNumber(mNativePointer, CtrSuppression.this));
        if (didWeekChange) {
            if (CtrSuppressionJni.get().hasPreviousWeekData(mNativePointer, CtrSuppression.this)) {
                int previousWeekImpressions = CtrSuppressionJni.get().getPreviousWeekImpressions(
                        mNativePointer, CtrSuppression.this);
                int previousWeekCtr = (int) (100
                        * CtrSuppressionJni.get().getPreviousWeekCtr(
                                mNativePointer, CtrSuppression.this));
                ContextualSearchUma.logPreviousWeekCtr(previousWeekImpressions, previousWeekCtr);
            }

            if (CtrSuppressionJni.get().hasPrevious28DayData(mNativePointer, CtrSuppression.this)) {
                int previous28DayImpressions = CtrSuppressionJni.get().getPrevious28DayImpressions(
                        mNativePointer, CtrSuppression.this);
                int previous28DayCtr = (int) (100
                        * CtrSuppressionJni.get().getPrevious28DayCtr(
                                mNativePointer, CtrSuppression.this));
                ContextualSearchUma.logPrevious28DayCtr(previous28DayImpressions, previous28DayCtr);
            }
        }
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap) {
            CtrSuppressionJni.get().recordImpression(
                    mNativePointer, CtrSuppression.this, wasSearchContentViewSeen);
        }
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder recorder) {
        if (CtrSuppressionJni.get().hasPreviousWeekData(mNativePointer, CtrSuppression.this)) {
            int previousWeekImpressions = CtrSuppressionJni.get().getPreviousWeekImpressions(
                    mNativePointer, CtrSuppression.this);
            int previousWeekCtr = (int) (100
                    * CtrSuppressionJni.get().getPreviousWeekCtr(
                            mNativePointer, CtrSuppression.this));
            recorder.logFeature(
                    ContextualSearchInteractionRecorder.Feature.PREVIOUS_WEEK_IMPRESSIONS_COUNT,
                    previousWeekImpressions);
            recorder.logFeature(
                    ContextualSearchInteractionRecorder.Feature.PREVIOUS_WEEK_CTR_PERCENT,
                    previousWeekCtr);
        }

        if (CtrSuppressionJni.get().hasPrevious28DayData(mNativePointer, CtrSuppression.this)) {
            int previous28DayImpressions = CtrSuppressionJni.get().getPrevious28DayImpressions(
                    mNativePointer, CtrSuppression.this);
            int previous28DayCtr = (int) (100
                    * CtrSuppressionJni.get().getPrevious28DayCtr(
                            mNativePointer, CtrSuppression.this));
            recorder.logFeature(
                    ContextualSearchInteractionRecorder.Feature.PREVIOUS_28DAY_IMPRESSIONS_COUNT,
                    previous28DayImpressions);
            recorder.logFeature(
                    ContextualSearchInteractionRecorder.Feature.PREVIOUS_28DAY_CTR_PERCENT,
                    previous28DayCtr);
        }
    }

    // ============================================================================================
    // Device integer storage.
    // ============================================================================================

    @CalledByNative
    void writeInt(String key, int value) {
        mPreferenceManager.writeInt(key, value);
    }

    @CalledByNative
    int readInt(String key) {
        return mPreferenceManager.readInt(key);
    }

    // ============================================================================================
    // Private helpers.
    // ============================================================================================

    /**
     * Updates the "current week number" preference and returns whether the week has changed.
     * @param currentWeekNumber The week number of the current week.
     * @return {@code true} if the current week number is different from the last time we checked,
     *         or we have never checked.
     */
    private boolean didWeekChange(int currentWeekNumber) {
        if (mPreferenceManager.readInt(ChromePreferenceKeys.CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER)
                == currentWeekNumber) {
            return false;
        } else {
            mPreferenceManager.writeInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER, currentWeekNumber);
            return true;
        }
    }

    // ============================================================================================
    // Native callback support.
    // ============================================================================================

    @CalledByNative
    private void clearNativePointer() {
        assert mNativePointer != 0;
        mNativePointer = 0;
    }

    // ============================================================================================
    // Native methods.
    // ============================================================================================

    @NativeMethods
    interface Natives {
        long init(CtrSuppression caller);

        void destroy(long nativeCtrSuppression, CtrSuppression caller);
        void recordImpression(long nativeCtrSuppression, CtrSuppression caller, boolean wasSeen);
        int getCurrentWeekNumber(long nativeCtrSuppression, CtrSuppression caller);
        boolean hasPreviousWeekData(long nativeCtrSuppression, CtrSuppression caller);
        int getPreviousWeekImpressions(long nativeCtrSuppression, CtrSuppression caller);
        float getPreviousWeekCtr(long nativeCtrSuppression, CtrSuppression caller);
        boolean hasPrevious28DayData(long nativeCtrSuppression, CtrSuppression caller);
        int getPrevious28DayImpressions(long nativeCtrSuppression, CtrSuppression caller);
        float getPrevious28DayCtr(long nativeCtrSuppression, CtrSuppression caller);
    }
}
