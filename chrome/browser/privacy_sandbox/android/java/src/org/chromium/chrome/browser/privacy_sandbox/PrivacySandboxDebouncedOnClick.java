// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/**
 * This class implements the OnClickListener and adds extra logic for debouncing clicks before
 * calling a locally-implemented onClick method.
 */
@NullMarked
public abstract class PrivacySandboxDebouncedOnClick implements View.OnClickListener {
    private long mLastClickRecordedTimestamp;
    // Debouncing value chosen after experimentation with 100ms - 400ms.
    private static final int MIN_ACTION_DISTANCE_MS = 200;

    private final String mNoticeName;

    public PrivacySandboxDebouncedOnClick(String noticeName) {
        mNoticeName = noticeName;
    }

    // OnClickListener
    @Override
    public final void onClick(View view) {
        long currTime = System.currentTimeMillis();

        if (shouldIgnoreClick(currTime)) {
            return;
        }

        // Record duration between non-ignored clicks.
        if (mLastClickRecordedTimestamp != 0) {
            RecordHistogram.recordTimesHistogram(
                    "PrivacySandbox.Notice.DurationSinceLastRegisteredAction." + mNoticeName,
                    currTime - mLastClickRecordedTimestamp);
        }

        mLastClickRecordedTimestamp = currTime;
        processClick(view);
    }

    /**
     * This method is called when a click is not ignored.
     *
     * @param view The view that was clicked.
     */
    public abstract void processClick(View view);

    private boolean shouldIgnoreClick(long currTime) {
        // If a click was registered in the last MIN_ACTION_DISTANCE ms, don't process the
        // incoming click.
        if (currTime - mLastClickRecordedTimestamp < MIN_ACTION_DISTANCE_MS) {
            return true;
        }

        return false;
    }
}
