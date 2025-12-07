// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.gfx.RectUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tracks and reports the percentage of coverage of AwContents on the root view. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@JNINamespace("android_webview")
@NullMarked
public class AwWindowCoverageTracker {
    private static final long RECALCULATION_DELAY_MS = 200;
    private static final String TAG = "AwContents";
    private static final boolean TRACE = false;

    @VisibleForTesting
    public static final Map<View, AwWindowCoverageTracker> sWindowCoverageTrackers =
            new HashMap<>();

    private final View mRootView;
    private final List<AwContents> mAwContentsList = new ArrayList<>();
    private long mRecalculationTime;
    private boolean mPendingRecalculation;

    private AwWindowCoverageTracker(View rootView) {
        mRootView = rootView;

        sWindowCoverageTrackers.put(rootView, this);
    }

    public static AwWindowCoverageTracker getOrCreateForRootView(
            AwContents contents, View rootView) {
        AwWindowCoverageTracker tracker = sWindowCoverageTrackers.get(rootView);

        if (tracker == null) {
            if (TRACE) {
                Log.i(TAG, "%s creating WindowCoverageTracker for %s", contents, rootView);
            }

            tracker = new AwWindowCoverageTracker(rootView);
        }

        return tracker;
    }

    public void trackContents(AwContents contents) {
        mAwContentsList.add(contents);
    }

    public void untrackContents(AwContents contents) {
        mAwContentsList.remove(contents);

        // If that was the last AwContents, remove ourselves from the static map.
        if (!isTracking()) {
            if (TRACE) {
                Log.i(TAG, "%s removing " + this, contents);
            }
            sWindowCoverageTrackers.remove(mRootView);
        }
    }

    private boolean isTracking() {
        return !mAwContentsList.isEmpty();
    }

    /**
     * Notifies this object that a recalculation of the window coverage is necessary.
     *
     * <p>This should be called every time any of the tracked AwContents changes its size,
     * visibility, or scheme.
     *
     * <p>Recalculation won't happen immediately, and will be rate limited.
     */
    public void onInputsUpdated() {
        long time = SystemClock.uptimeMillis();

        if (mPendingRecalculation) {
            return;
        }
        mPendingRecalculation = true;

        if (time > mRecalculationTime + RECALCULATION_DELAY_MS) {
            // Enough time has elapsed since the last recalculation, run it now.
            mRecalculationTime = time;
        } else {
            // Not enough time has elapsed, run it once enough time has elapsed.
            mRecalculationTime += RECALCULATION_DELAY_MS;
        }

        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    recalculate();
                    mPendingRecalculation = false;
                },
                mRecalculationTime - time);
    }

    private static int[] toIntArray(List<Integer> list) {
        int[] array = new int[list.size()];
        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }
        return array;
    }

    private void recalculate() {
        if (TRACE) {
            Log.i(TAG, "%s recalculate", this);
        }

        List<Rect> contentRects = new ArrayList<>();

        Rect rootVisibleRect =
                new Rect(
                        (int) mRootView.getX(),
                        (int) mRootView.getY(),
                        (int) mRootView.getX() + mRootView.getWidth(),
                        (int) mRootView.getY() + mRootView.getHeight());
        int rootArea = RectUtils.getRectArea(rootVisibleRect);

        int globalPercentage = 0;

        // Note that a scheme could occur more than once at a time.
        List<String> schemes = new ArrayList<>();
        List<Integer> schemePercentages = new ArrayList<>();

        // If the root view has a width or height of 0 then nothing is visible, so leave the
        // lists empty and pass them on like that. Also, we don't want to divide by 0.
        if (rootArea > 0) {
            for (AwContents content : mAwContentsList) {
                Rect contentRect = content.getRectForWindowCoverage();

                if (contentRect == null) continue;

                // If the intersect method returns true then it may have modified
                // contentRect. A Rect with area 0 will not intersect with anything.
                if (contentRect.intersect(rootVisibleRect)) {
                    contentRects.add(contentRect);
                    schemes.add(content.getScheme());
                    schemePercentages.add(RectUtils.getRectArea(contentRect) * 100 / rootArea);
                }
            }

            globalPercentage =
                    RectUtils.calculatePixelsOfCoverage(rootVisibleRect, contentRects)
                            * 100
                            / rootArea;
        }

        AwWindowCoverageTrackerJni.get()
                .updateScreenCoverage(
                        globalPercentage,
                        schemes.toArray(new String[schemes.size()]),
                        toIntArray(schemePercentages));
    }

    @NativeMethods
    interface Natives {
        void updateScreenCoverage(int globalPercentage, String[] schemes, int[] schemePercentages);
    }
}
