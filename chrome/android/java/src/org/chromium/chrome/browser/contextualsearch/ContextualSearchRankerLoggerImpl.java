// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.Map;

/**
 * Implements the UMA logging for Ranker that's used for Contextual Search Tap Suppression.
 */
public class ContextualSearchRankerLoggerImpl implements ContextualSearchInteractionRecorder {
    // Pointer to the native instance of this class.
    private long mNativePointer;

    // Whether logging for the current page has been setup.
    private boolean mIsLoggingReadyForPage;

    // The WebContents of the base page that the log data is associated with.
    private WebContents mBasePageWebContents;

    // Whether inference has already occurred for this interaction (and calling #logFeature is no
    // longer allowed).
    private boolean mHasInferenceOccurred;

    // What kind of ML prediction we were able to get.
    private @AssistRankerPrediction int mAssistRankerPrediction =
            AssistRankerPrediction.UNDETERMINED;

    // Map that accumulates all of the Features to log for a specific user-interaction.
    private Map<Integer /* @Feature */, Object> mFeaturesToLog;

    // A for-testing copy of all the features to log setup so that it will survive a {@link #reset}.
    private Map<Integer /* @Feature */, Object> mOutcomesLoggedForTesting;

    private ContextualSearchInteractionPersister mInteractionPersister;

    private long mEventIdToPersist;

    /**
     * Constructs a Ranker Logger and associated native implementation to write Contextual Search
     * ML data to Ranker.
     */
    public ContextualSearchRankerLoggerImpl() {
        this(new ContextualSearchInteractionPersisterImpl());
    }

    /**
     * Constructs a Ranker Logger implementation for testing.
     * @param interactionPersister The {@link ContextualSearchInteractionPersister} to use for this
     *        instance.
     */
    public ContextualSearchRankerLoggerImpl(
            ContextualSearchInteractionPersister interactionPersister) {
        mInteractionPersister = interactionPersister;
        if (isEnabled()) {
            mNativePointer = ContextualSearchRankerLoggerImplJni.get().init(
                    ContextualSearchRankerLoggerImpl.this);
        }
    }

    /**
     * Gets the name of the given outcome.
     * @param feature A feature whose name we want.
     * @return The name of the outcome if the give parameter is an outcome, or {@code null} if it's
     *         not.
     */
    @VisibleForTesting
    protected static final String outcomeName(@Feature int feature) {
        switch (feature) {
            case Feature.OUTCOME_WAS_PANEL_OPENED:
                return "OutcomeWasPanelOpened";
            case Feature.OUTCOME_WAS_QUICK_ACTION_CLICKED:
                return "OutcomeWasQuickActionClicked";
            case Feature.OUTCOME_WAS_QUICK_ANSWER_SEEN:
                return "OutcomeWasQuickAnswerSeen";
            // UKM CS v2 outcomes.
            case Feature.OUTCOME_WAS_CARDS_DATA_SHOWN:
                return "OutcomeWasCardsDataShown";
            default:
                return null;
        }
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.  The ContextualSearchRankerLoggerImplJni.get().destroy will call the
     * destructor on the native instance.
     */
    void destroy() {
        // TODO(donnd): looks like this is never being called.  Fix.
        if (isEnabled()) {
            assert mNativePointer != 0;
            writeLogAndReset();
            ContextualSearchRankerLoggerImplJni.get().destroy(
                    mNativePointer, ContextualSearchRankerLoggerImpl.this);
            mNativePointer = 0;
        }
        mIsLoggingReadyForPage = false;
    }

    @Override
    public void setupLoggingForPage(@Nullable WebContents basePageWebContents) {
        mIsLoggingReadyForPage = true;
        mBasePageWebContents = basePageWebContents;
        mHasInferenceOccurred = false;
        ContextualSearchRankerLoggerImplJni.get().setupLoggingAndRanker(
                mNativePointer, ContextualSearchRankerLoggerImpl.this, basePageWebContents);
    }

    @Override
    public boolean isQueryEnabled() {
        return ContextualSearchRankerLoggerImplJni.get().isQueryEnabled(
                mNativePointer, ContextualSearchRankerLoggerImpl.this);
    }

    @Override
    public void logOutcome(@Feature int feature, Object value) {
        assert mIsLoggingReadyForPage;
        assert mHasInferenceOccurred;
        if (!isEnabled()) return;

        logInternal(feature, value);
    }

    @Override
    public @AssistRankerPrediction int runPredictionForTapSuppression() {
        assert mIsLoggingReadyForPage;
        assert !mHasInferenceOccurred;
        mHasInferenceOccurred = true;
        if (isEnabled() && mBasePageWebContents != null) {
            mAssistRankerPrediction = ContextualSearchRankerLoggerImplJni.get().runInference(
                    mNativePointer, ContextualSearchRankerLoggerImpl.this);
            ContextualSearchUma.logRecordedFeaturesToRanker();
        }
        return mAssistRankerPrediction;
    }

    @Override
    public @AssistRankerPrediction int getPredictionForTapSuppression() {
        return mAssistRankerPrediction;
    }

    @Override
    public void reset() {
        mIsLoggingReadyForPage = false;
        mHasInferenceOccurred = false;
        mFeaturesToLog = null;
        mBasePageWebContents = null;
        mAssistRankerPrediction = AssistRankerPrediction.UNDETERMINED;
    }

    @Override
    public void writeLogAndReset() {
        if (isEnabled()) {
            if (mBasePageWebContents != null && mFeaturesToLog != null
                    && !mFeaturesToLog.isEmpty()) {
                assert mIsLoggingReadyForPage;
                assert mHasInferenceOccurred;

                mOutcomesLoggedForTesting = mFeaturesToLog;
                ContextualSearchUma.logRecordedOutcomesToRanker();
                // Also persist the outcomes if we are persisting this interaction.
                if (mEventIdToPersist != 0) {
                    mInteractionPersister.persistInteractions(mEventIdToPersist, mFeaturesToLog);
                    mEventIdToPersist = 0;
                }
            }
            ContextualSearchRankerLoggerImplJni.get().writeLogAndReset(
                    mNativePointer, ContextualSearchRankerLoggerImpl.this);
        }
        reset();
    }

    @Override
    public void persistInteraction(long eventId) {
        if (eventId != 0) mEventIdToPersist = eventId;
    }

    @Override
    public ContextualSearchInteractionPersister getInteractionPersister() {
        return mInteractionPersister;
    }

    /**
     * Logs the given feature/value to the internal map that accumulates an entire record (which can
     * be logged by calling writeLogAndReset).
     * @param feature The feature to log.
     * @param value The value to log.
     */
    private void logInternal(@Feature int feature, Object value) {
        if (mFeaturesToLog == null) mFeaturesToLog = new HashMap<Integer, Object>();
        mFeaturesToLog.put(feature, value);
    }

    /** Whether actually writing data is enabled.  If not, we may do nothing, or just print. */
    private boolean isEnabled() {
        return !ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_UKM_RANKER_LOGGING_DISABLED);
    }

    /**
     * Gets the current set of outcomes that have been logged.  Should only be used for
     * testing purposes!
     * @return The current set of outcomes that have been logged, or {@code null}.
     */
    @VisibleForTesting
    @Nullable
    Map<Integer, Object> getOutcomesLogged() {
        return mOutcomesLoggedForTesting;
    }

    // ============================================================================================
    // Native methods.
    // ============================================================================================

    @NativeMethods
    interface Natives {
        long init(ContextualSearchRankerLoggerImpl caller);

        void destroy(long nativeContextualSearchRankerLoggerImpl,
                ContextualSearchRankerLoggerImpl caller);
        void setupLoggingAndRanker(long nativeContextualSearchRankerLoggerImpl,
                ContextualSearchRankerLoggerImpl caller, WebContents basePageWebContents);
        // Returns an AssistRankerPrediction integer value.
        int runInference(long nativeContextualSearchRankerLoggerImpl,
                ContextualSearchRankerLoggerImpl caller);

        void writeLogAndReset(long nativeContextualSearchRankerLoggerImpl,
                ContextualSearchRankerLoggerImpl caller);
        boolean isQueryEnabled(long nativeContextualSearchRankerLoggerImpl,
                ContextualSearchRankerLoggerImpl caller);
    }
}
