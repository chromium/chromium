// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Map;

/** This Controller for the auxiliary search. */
public interface AuxiliarySearchController extends PauseResumeWithNativeObserver {
    @IntDef({
        AuxiliarySearchDataType.ALL,
        AuxiliarySearchDataType.BOOKMARK,
        AuxiliarySearchDataType.TAB,
        AuxiliarySearchDataType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AuxiliarySearchDataType {
        int ALL = 0;
        int BOOKMARK = 1;
        int TAB = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Registers to the given lifecycle dispatcher.
     *
     * @param lifecycleDispatcher tracks the lifecycle of the Activity of pause and resume.
     */
    default void register(ActivityLifecycleDispatcher lifecycleDispatcher) {}

    @Override
    default void onResumeWithNative() {}

    @Override
    default void onPauseWithNative() {}

    /** Destroy and unhook objects at destruction. */
    default void destroy() {}

    /**
     * Called after the background task has fetched metadata.
     *
     * @param tabs The tabs to donate.
     * @param tabIdToFaviconMap A map of <TabId, Bitmap>.
     * @param callback The callback to notify whether the donation is succeed.
     * @param startTimeMs The starting time in milliseconds.
     */
    default void onBackgroundTaskStart(
            @NonNull List<AuxiliarySearchEntry> tabs,
            @NonNull Map<Integer, Bitmap> tabIdToFaviconMap,
            @NonNull Callback<Boolean> callback,
            long startTimeMs) {}
}
