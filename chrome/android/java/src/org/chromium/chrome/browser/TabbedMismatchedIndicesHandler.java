// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;

import java.util.function.LongSupplier;

/** Implementation of MismatchedIndicesHandler used by {@link ChromeTabbedActivity}. */
@NullMarked
public class TabbedMismatchedIndicesHandler implements MismatchedIndicesHandler {
    private final LongSupplier mOnCreateTimestampMsSupplier;
    private final boolean mSkipIndexReassignment;

    public TabbedMismatchedIndicesHandler(
            LongSupplier onCreateTimestampMsSupplier, boolean skipIndexReassignment) {
        mOnCreateTimestampMsSupplier = onCreateTimestampMsSupplier;
        mSkipIndexReassignment = skipIndexReassignment;
    }

    @Override
    public boolean handleMismatchedIndices(
            Activity activityAtRequestedIndex,
            boolean isActivityInAppTasks,
            boolean isActivityInSameTask) {
        boolean shouldHandleMismatch =
                activityAtRequestedIndex.isFinishing()
                        || isActivityInSameTask
                        || !isActivityInAppTasks;

        if (!shouldHandleMismatch
                || !(activityAtRequestedIndex
                        instanceof ChromeTabbedActivity tabbedActivityAtRequestedIndex)) {
            return false;
        }

        // Destroy the TabPersistentStore instance maintained by the activity at the requested
        // index. Save the tab state first to align with the current flow of execution when the
        // store is destroyed.
        var tabModelOrchestrator =
                assertNonNull(
                        tabbedActivityAtRequestedIndex.getTabModelOrchestratorSupplier().get());
        // If the two activities launched within a short span, simply destroy the persistent store
        // instance of the activity at the requested index, assuming no changes have been made to
        // the tab state during this time.
        long onCreateTimeDeltaMs =
                mOnCreateTimestampMsSupplier.getAsLong()
                        - tabbedActivityAtRequestedIndex.getOnCreateTimestampMs();
        boolean shouldSaveState =
                tabbedActivityAtRequestedIndex.getLifecycleDispatcher().getCurrentActivityState()
                        < ActivityLifecycleDispatcher.ActivityState.STOPPED_WITH_NATIVE;
        if (shouldSaveState
                && onCreateTimeDeltaMs
                        > ChromeFeatureList
                                .sTabWindowManagerReportIndicesMismatchTimeDiffThresholdMs
                                .getValue()) {
            // Save state only if #onStopWithNative() that invokes this, has not run yet.
            tabModelOrchestrator.getTabPersistentStore().saveState();
        }
        tabModelOrchestrator.destroyTabPersistentStore();

        // If the activity at the requested index is not finishing already, explicitly finish it.
        if (!activityAtRequestedIndex.isFinishing()) {
            activityAtRequestedIndex.finish();
        }
        return true;
    }

    @Override
    public boolean skipIndexReassignment() {
        return mSkipIndexReassignment;
    }
}
