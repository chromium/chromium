// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper class to record UMA metrics for the Tab Bottom Sheet. */
@NullMarked
public class TabBottomSheetMetrics {

    // LINT.IfChange(Transition)
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Must match the TabBottomSheetStateTransition enum in
    // tools/metrics/histograms/metadata/android/enums.xml
    @IntDef({
        Transition.UNKNOWN,
        Transition.PEEK_TO_HALF,
        Transition.PEEK_TO_FULL,
        Transition.HALF_TO_PEEK,
        Transition.HALF_TO_FULL,
        Transition.FULL_TO_PEEK,
        Transition.FULL_TO_HALF
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Transition {
        int UNKNOWN = 0;
        int PEEK_TO_HALF = 1;
        int PEEK_TO_FULL = 2;
        int HALF_TO_PEEK = 3;
        int HALF_TO_FULL = 4;
        int FULL_TO_PEEK = 5;
        int FULL_TO_HALF = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:TabBottomSheetStateTransition)

    /**
     * Records a stable state hit.
     *
     * @param clientType The type of client using the bottom sheet.
     * @param state The stable SheetState entered.
     */
    public static void recordStateHit(
            @TabBottomSheetClientType int clientType, @SheetState int state) {
        if (state == SheetState.NONE) {
            return;
        }
        String prefix = getHistogramPrefix(clientType);
        if (prefix != null) {
            RecordHistogram.recordEnumeratedHistogram(
                    prefix + ".CurrentState", state, SheetState.SCROLLING + 1);
        }
    }

    /**
     * Records a transition between stable open states (PEEK, HALF, FULL).
     *
     * @param clientType The type of client using the bottom sheet.
     * @param from The stable SheetState transitioned from.
     * @param to The stable SheetState transitioned to.
     */
    public static void recordTransition(
            @TabBottomSheetClientType int clientType, @SheetState int from, @SheetState int to) {
        if (isOpenStableState(from) && isOpenStableState(to)) {
            @Transition int transition = getTransitionValue(from, to);
            if (transition != Transition.UNKNOWN) {
                String prefix = getHistogramPrefix(clientType);
                if (prefix != null) {
                    RecordHistogram.recordEnumeratedHistogram(
                            prefix + ".StateTransition", transition, Transition.NUM_ENTRIES);
                }
            }
        }
    }

    /**
     * Records the state change reason for entering PEEK or HIDDEN states.
     *
     * @param clientType The type of client using the bottom sheet.
     * @param state The state entered (must be PEEK or HIDDEN).
     * @param reason The reason for the state change.
     */
    public static void recordStateChangeReason(
            @TabBottomSheetClientType int clientType,
            @SheetState int state,
            @StateChangeReason int reason) {
        String stateName = null;
        if (state == SheetState.PEEK) {
            stateName = "Peek";
        } else if (state == SheetState.HIDDEN) {
            stateName = "Hidden";
        }

        if (stateName != null) {
            String prefix = getHistogramPrefix(clientType);
            if (prefix != null) {
                RecordHistogram.recordEnumeratedHistogram(
                        prefix + ".StateChangeReason." + stateName,
                        reason,
                        StateChangeReason.MAX_VALUE + 1);
            }
        }
    }

    private static boolean isOpenStableState(@SheetState int state) {
        return state == SheetState.PEEK || state == SheetState.HALF || state == SheetState.FULL;
    }

    private static @Transition int getTransitionValue(@SheetState int from, @SheetState int to) {
        switch (from) {
            case SheetState.PEEK:
                switch (to) {
                    case SheetState.HALF:
                        return Transition.PEEK_TO_HALF;
                    case SheetState.FULL:
                        return Transition.PEEK_TO_FULL;
                }
                break;
            case SheetState.HALF:
                switch (to) {
                    case SheetState.PEEK:
                        return Transition.HALF_TO_PEEK;
                    case SheetState.FULL:
                        return Transition.HALF_TO_FULL;
                }
                break;
            case SheetState.FULL:
                switch (to) {
                    case SheetState.PEEK:
                        return Transition.FULL_TO_PEEK;
                    case SheetState.HALF:
                        return Transition.FULL_TO_HALF;
                }
                break;
        }
        return Transition.UNKNOWN;
    }

    private static @Nullable String getHistogramPrefix(@TabBottomSheetClientType int clientType) {
        if (clientType == TabBottomSheetClientType.GLIC) {
            return "Android.TabBottomSheet.Glic";
        } else if (clientType == TabBottomSheetClientType.CONTEXTUAL_TASKS) {
            return "Android.TabBottomSheet.ContextualTasks";
        }
        return null;
    }
}
