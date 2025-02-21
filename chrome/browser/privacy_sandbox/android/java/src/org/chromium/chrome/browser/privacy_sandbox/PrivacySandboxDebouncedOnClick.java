// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * This class implements the OnClickListener and adds extra logic for debouncing clicks before
 * calling a locally-implemented onClick method.
 */
public abstract class PrivacySandboxDebouncedOnClick implements View.OnClickListener {
    private long mLastClickRecordedTimestamp;
    // Value based on initial hunch for how long we want to ignore click actions.
    private static final int DEFAULT_MIN_ACTION_DISTANCE_MS = 200;
    private static final int MIN_ACTION_DISTANCE_MS =
            ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.PRIVACY_SANDBOX_NOTICE_ACTION_DEBOUNCING_ANDROID,
                    "debouncing-delay-ms",
                    DEFAULT_MIN_ACTION_DISTANCE_MS);

    private String mNoticeName;

    public PrivacySandboxDebouncedOnClick(String noticeName) {
        mNoticeName = noticeName;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(PrivacySandboxClickEventAndroid)
    @IntDef({
        ClickEvent.ACK,
        ClickEvent.SETTINGS,
        ClickEvent.MORE,
        ClickEvent.DROP_DOWN,
        ClickEvent.NO,
        ClickEvent.PRIVACY_POLICY_BACK
    })
    public static @interface ClickEvent {
        int ACK = 0;
        int SETTINGS = 1;
        int MORE = 2;
        int DROP_DOWN = 3;
        int NO = 4;
        int PRIVACY_POLICY_BACK = 5;
        int MAX_VALUE = PRIVACY_POLICY_BACK;
    }

    // LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivacySandboxClickEvent)

    // OnClickListener
    @Override
    public final void onClick(View view) {
        long currTime = System.currentTimeMillis();

        if (shouldIgnoreClick(currTime)) {
            emitClickIgnoredHistogram(view);
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
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_NOTICE_ACTION_DEBOUNCING_ANDROID)) {
            return false;
        }
        // If a click was registered in the last MIN_ACTION_DISTANCE ms, don't process the
        // incoming click.
        if (currTime - mLastClickRecordedTimestamp < MIN_ACTION_DISTANCE_MS) {
            return true;
        }

        return false;
    }

    // TODO(crbug.com/395862255): Remove histogram once debouncing is verified.
    private void emitClickIgnoredHistogram(View view) {
        int id = view.getId();

        @ClickEvent int bucketNum = ClickEvent.MAX_VALUE + 1;

        if (id == R.id.ack_button || id == R.id.ack_button_equalized) {
            bucketNum = ClickEvent.ACK;
        } else if (id == R.id.settings_button) {
            bucketNum = ClickEvent.SETTINGS;
        } else if (id == R.id.more_button) {
            bucketNum = ClickEvent.MORE;
        } else if (id == R.id.dropdown_element) {
            bucketNum = ClickEvent.DROP_DOWN;
        } else if (id == R.id.no_button) {
            bucketNum = ClickEvent.NO;
        } else if (id == R.id.privacy_policy_back_button) {
            bucketNum = ClickEvent.PRIVACY_POLICY_BACK;
        }

        // If a valid bucketNum has been set by a known Rid, log a histogram entry.
        if (bucketNum <= ClickEvent.MAX_VALUE) {
            RecordHistogram.recordEnumeratedHistogram(
                    "PrivacySandbox.Notice.ClickIgnored." + mNoticeName,
                    bucketNum,
                    ClickEvent.MAX_VALUE);
        }
    }
}
