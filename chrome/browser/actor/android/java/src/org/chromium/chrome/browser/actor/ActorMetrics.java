// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper class for recording Actor-related UMA metrics. */
@NullMarked
public class ActorMetrics {

    // LINT.IfChange(ActorPipStatus)

    @IntDef({ActorPipStatus.ENTERED, ActorPipStatus.EXITED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipStatus {
        int ENTERED = 0;
        int EXITED = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//chrome/browser/actor/actor_metrics.cc:ActorPipStatus)

    // LINT.IfChange(ActorPipExitReason)

    @IntDef({
        ActorPipExitReason.CLOSE,
        ActorPipExitReason.EXPAND,
        ActorPipExitReason.COMPLETED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipExitReason {
        int CLOSE = 0;
        int EXPAND = 1;
        int COMPLETED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//chrome/browser/actor/actor_metrics.cc:ActorPipExitReason)

    // LINT.IfChange(ActorPipUserInteraction)

    @IntDef({
        ActorPipUserInteraction.PAUSE,
        ActorPipUserInteraction.RESUME,
        ActorPipUserInteraction.EXPAND
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipUserInteraction {
        int PAUSE = 0;
        int RESUME = 1;
        int EXPAND = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//chrome/browser/actor/actor_metrics.cc:ActorPipUserInteraction)

    @IntDef({
        ActorPauseResumeSource.PIP,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPauseResumeSource {
        int PIP = 0;
    }

    /** Records the PiP status (Enter/Exit). */
    public static void recordPipStatus(@ActorPipStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.Status", status, ActorPipStatus.NUM_ENTRIES);
    }

    /** Records the PiP exit reason. */
    public static void recordPipExitReason(@ActorPipExitReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.ExitReason", reason, ActorPipExitReason.NUM_ENTRIES);
    }

    /** Records the PiP duration. */
    public static void recordPipDuration(long durationMs) {
        RecordHistogram.recordLongTimesHistogram("Actor.Pip.Duration", durationMs);
    }

    /** Records an interaction with the PiP window. */
    public static void recordPipUserInteraction(@ActorPipUserInteraction int interaction) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.UserInteractions", interaction, ActorPipUserInteraction.NUM_ENTRIES);
    }
}
