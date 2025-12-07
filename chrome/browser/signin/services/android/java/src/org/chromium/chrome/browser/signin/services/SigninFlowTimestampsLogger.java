// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.StringDef;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that logs the timestamp of different events in the sign-in flow. The timestamps are
 * recorded as durations from the time of creation of the logger.
 */
@NullMarked
public class SigninFlowTimestampsLogger {

    // LINT.IfChange(SigninFlowTimestamps)
    /**
     * Events in the sign-in flow. These values are part of the
     * Signin.Timestamps.{FlowVariant}.{Event} histogram.
     */
    @StringDef({Event.MANAGEMENT_STATUS_LOADED, Event.SIGNIN_COMPLETED, Event.SIGNIN_ABORTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Event {
        String MANAGEMENT_STATUS_LOADED = "ManagementStatusLoaded";
        String SIGNIN_COMPLETED = "SigninCompleted";
        String SIGNIN_ABORTED = "SigninAborted";
    }

    /**
     * Type of the sign-in flow. These values are part of the
     * Signin.Timestamps.{FlowVariant}.{Event} histogram.
     */
    @StringDef({FlowVariant.FULLSCREEN, FlowVariant.WEB, FlowVariant.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FlowVariant {
        String FULLSCREEN = "Fullscreen";
        String WEB = "Web";
        String OTHER = "Other";
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/signin/histograms.xml:SigninFlowTimestamps)

    private final @FlowVariant String mFlowVariant;
    private final long mFlowStartTime;
    private long mManagementNoticeShownTime;
    private long mManagementConfirmationDelay;

    public static SigninFlowTimestampsLogger startLogging(@FlowVariant String flowVariant) {
        return new SigninFlowTimestampsLogger(flowVariant);
    }

    /**
     * @param signinAccessPoint The access point that triggered the sign-in flow.
     */
    private SigninFlowTimestampsLogger(@FlowVariant String flowVariant) {
        mFlowVariant = flowVariant;
        mFlowStartTime = TimeUtils.elapsedRealtimeMillis();
    }

    /**
     * Records the timestamp when the management notice is shown to the user. If the management
     * notice is shown, this method should be called when the notice is shown and
     * onManagementNoticeAccepted() should be called when the notice is accepted.
     */
    public void onManagementNoticeShown() {
        mManagementNoticeShownTime = TimeUtils.elapsedRealtimeMillis();
    }

    /**
     * Records the time the user took to accept the management notice. If the management notice is
     * shown, onManagementNoticeShown() should be called when the notice is shown and this method
     * should be called when the notice is accepted.
     */
    public void onManagementNoticeAccepted() {
        assert mManagementNoticeShownTime != 0;
        mManagementConfirmationDelay =
                TimeUtils.elapsedRealtimeMillis() - mManagementNoticeShownTime;
    }

    /**
     * Records the timestamp of a sign-in event.
     *
     * @param event The event to record.
     */
    public void recordTimestamp(@Event String event) {
        long duration = TimeUtils.elapsedRealtimeMillis() - mFlowStartTime;
        if (event.equals(Event.SIGNIN_COMPLETED) || event.equals(Event.SIGNIN_ABORTED)) {
            assert mManagementConfirmationDelay > 0 || mManagementNoticeShownTime == 0;
            duration -= mManagementConfirmationDelay;
        }
        final String histogramName = "Signin.SignIn.Timestamps." + mFlowVariant + "." + event;
        RecordHistogram.recordTimesHistogram(histogramName, duration);
    }
}
