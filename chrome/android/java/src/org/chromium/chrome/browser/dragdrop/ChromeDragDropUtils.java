// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.content.Context;
import android.content.Intent;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.ui.widget.Toast;

/** Utility class for Chrome drag and drop implementations. */
public class ChromeDragDropUtils {
    private static final int MAX_TAB_OR_GROUP_TEARING_FAILURE_COUNT_PER_DAY = 10;

    /**
     * Records linear histogram Android.DragDrop.TabOrGroup.MaxInstanceFailureCount and saves
     * related SharedPreferences values.
     */
    public static void recordTabOrGroupDragToCreateInstanceFailureCount() {
        var prefs = ChromeSharedPreferences.getInstance();
        // Check the failure count in a day for every unhandled dragged tab or group drop when max
        // instances
        // are open.
        long timestamp =
                prefs.readLong(
                        ChromePreferenceKeys
                                .TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS,
                        0);
        int failureCount =
                prefs.readInt(
                        ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_COUNT, 0);
        long current = System.currentTimeMillis();

        boolean isNewDay = timestamp == 0 || current - timestamp > DateUtils.DAY_IN_MILLIS;
        if (isNewDay) {
            prefs.writeLong(
                    ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS,
                    current);
            // Reset the count to 0 if it is the start of the next 24-hour period.
            failureCount = 0;
        }

        RecordHistogram.recordExactLinearHistogram(
                "Android.DragDrop.TabOrGroup.MaxInstanceFailureCount",
                failureCount + 1,
                MAX_TAB_OR_GROUP_TEARING_FAILURE_COUNT_PER_DAY + 1);
        prefs.writeInt(
                ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_COUNT,
                failureCount + 1);
    }

    /**
     * Gets the {@link DragDropType} for dragged data that uses an intent to create a new Chrome
     * instance.
     *
     * @param intent The source intent.
     * @return The {@link DragDropType} of the dragged data.
     */
    public static @DragDropType int getDragDropTypeFromIntent(Intent intent) {
        return switch (intent.getIntExtra(
                IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN)) {
            case UrlIntentSource.LINK -> DragDropType.LINK_TO_NEW_INSTANCE;
            case UrlIntentSource.TAB_IN_STRIP, UrlIntentSource.TAB_GROUP_IN_STRIP -> DragDropType
                    .TAB_STRIP_TO_NEW_INSTANCE;
            default -> DragDropType.UNKNOWN_TO_NEW_INSTANCE;
        };
    }

    /**
     * Determines the destination index when a tab is dropped into a different model.
     *
     * @param context The application context.
     * @param isSourceIncognito Whether the source tab is in incognito mode.
     * @param selector The current {@link TabModelSelector} to act on.
     * @return The index where the tab should be inserted in the destination model.
     */
    public static int handleDropInDifferentModel(
            Context context, boolean isSourceIncognito, TabModelSelector selector) {
        assert selector != null;

        // get Current selected tab in destination window.
        Tab destTab = selector.getCurrentTab();

        // Determine the destination index for drop. If the source and destination window belong to
        // different models, show toast place the dragged view at the end of destination model.
        // Otherwise place it immediately after the selected tab.
        final int destIndex;
        if (isSourceIncognito == destTab.isIncognitoBranded()) {
            destIndex =
                    TabModelUtils.getTabIndexById(
                                    selector.getModel(destTab.isIncognitoBranded()),
                                    destTab.getId())
                            + 1;
        } else {
            destIndex = selector.getModel(isSourceIncognito).getCount();
            Toast.makeText(context, R.string.tab_dropped_different_model, Toast.LENGTH_LONG).show();
        }
        return destIndex;
    }

    /**
     * Retrieves {@link TabGroupMetadata} from the global drag-and-drop state.
     *
     * @param globalState The {@link DragDropGlobalState} containing drag data.
     * @return The {@link TabGroupMetadata} if available, otherwise {@code null}.
     */
    public static @Nullable TabGroupMetadata getTabGroupMetadataFromGlobalState(
            @Nullable DragDropGlobalState globalState) {
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null
                : "Attempting to access dragged tab group with invalid drag state.";
        if (!(globalState.getData() instanceof ChromeTabGroupDropDataAndroid)) return null;
        return ((ChromeTabGroupDropDataAndroid) globalState.getData()).tabGroupMetadata;
    }

    /**
     * Retrieves a {@link Tab} from the global drag-and-drop state.
     *
     * @param globalState The {@link DragDropGlobalState} containing drag data.
     * @return The {@link Tab} if available, otherwise {@code null}.
     */
    public static @Nullable Tab getTabFromGlobalState(@Nullable DragDropGlobalState globalState) {
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null : "Attempting to access dragged tab with invalid drag state.";
        if (!(globalState.getData() instanceof ChromeTabDropDataAndroid)) return null;
        return ((ChromeTabDropDataAndroid) globalState.getData()).tab;
    }

    /**
     * Checks whether the dragged tab is part of a tab group.
     *
     * @param globalState The {@link DragDropGlobalState} containing drag data.
     * @return {@code true} if the dragged tab is part of a tab group, {@code false} otherwise.
     */
    public static boolean isTabInGroupFromGlobalState(@NonNull DragDropGlobalState globalState) {
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null : "Attempting to access dragged tab with invalid drag state.";
        if (globalState.getData() instanceof ChromeTabDropDataAndroid) {
            return ((ChromeTabDropDataAndroid) globalState.getData()).isTabInGroup;
        } else {
            return false;
        }
    }
}
