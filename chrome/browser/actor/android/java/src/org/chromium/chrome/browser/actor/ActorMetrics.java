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

    @IntDef({ActorPipStatus.ENTERED, ActorPipStatus.EXITED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActorPipStatus {
        int ENTERED = 0;
        int EXITED = 1;
        int NUM_ENTRIES = 2;
    }

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

    /** Records an interaction with the PiP window. */
    public static void recordPipUserInteraction(@ActorPipUserInteraction int interaction) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.Pip.UserInteractions", interaction, ActorPipUserInteraction.NUM_ENTRIES);
    }
}
