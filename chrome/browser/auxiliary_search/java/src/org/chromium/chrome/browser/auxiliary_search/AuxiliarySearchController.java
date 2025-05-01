// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Map;

/** This Controller for the auxiliary search. */
@NullMarked
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

    @IntDef({
        AuxiliarySearchHostType.CTA,
        AuxiliarySearchHostType.CTA_BACKGROUND_TASK,
        AuxiliarySearchHostType.CCT_BACKGROUND_TASK,
        AuxiliarySearchHostType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AuxiliarySearchHostType {
        int CTA = 0;
        int CTA_BACKGROUND_TASK = 1;
        int CCT_BACKGROUND_TASK = 2;
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
    default void destroy(@Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {}

    /**
     * Called after the background task has fetched metadata.
     *
     * @param entries The tabs to donate.
     * @param entryToFaviconMap A map of donation entry and favicon.
     * @param callback The callback to notify whether the donation is succeed.
     * @param startTimeMs The starting time in milliseconds.
     * @param <T> The type of the entry data for donation.
     */
    default <T> void onBackgroundTaskStart(
            List<T> entries,
            Map<T, Bitmap> entryToFaviconMap,
            Callback<Boolean> callback,
            long startTimeMs) {}

    /**
     * Called in ChromeTabbedActivity#onDeferredStartup() after critical initialization in cold
     * startup.
     */
    default void onDeferredStartup() {}
}
