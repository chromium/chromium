// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.version_info.VersionConstants;
import org.chromium.chrome.browser.omaha.UpdateConfigs;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A helper class for tracking whether or not an update was successful.  This tracker works across
 * restarts, as update success cannot be immediately determined.
 */
public class UpdateSuccessMetrics {
    /** The type of update currently running.  Used for identifying which metric to tag. */
    public static int INTENT_UPDATE_TYPE = 1;

    /** How we are attributing the success. */
    @IntDef({AttributionType.SESSION, AttributionType.TIME_WINDOW})
    @Retention(RetentionPolicy.SOURCE)
    @interface AttributionType {
        /**
         * Success is determined by looking at whether or not the version at the start of an update
         * is different from the current version, assuming we are not still updating.  This happens
         * the first time we detect that an update is not currently active (e.g. next session).
         */
        int SESSION = 0;

        /**
         * Success is determined by looking at whether or not the version at the start of an update
         * is different from the current version, assuming we are not still updating, based on a
         * time window.  This means that if an update is successful within a specific window, even
         * if it does not happen immediately, it is flagged as success.  If the window expires
         * without an update, it is considered a failure.
         */
        int TIME_WINDOW = 1;
    }

    private final TrackingProvider mProvider;

    /** Creates an instance of UpdateSuccessMetrics. */
    public UpdateSuccessMetrics() {
        this(new TrackingProvider());
    }

    /**
     * Creates an instance of UpdateSuccessMetrics.
     * @param provider The {@link TrackingProvider} to use.  This is meant to facilitate testing.
     */
    @VisibleForTesting
    UpdateSuccessMetrics(TrackingProvider provider) {
        mProvider = provider;
    }

    /** To be called right before we are about to interact with the Play Store for an update. */
    public void startUpdate() {
        mProvider
                .get()
                .then(
                        state -> {
                            // We're using System.currentTimeMillis() here to track time across
                            // restarts.
                            Tracking info =
                                    Tracking.newBuilder()
                                            .setTimestampMs(System.currentTimeMillis())
                                            .setVersion(VersionConstants.PRODUCT_VERSION)
                                            .setType(Type.INTENT)
                                            .setSource(Source.FROM_MENU)
                                            .setRecordedSession(false)
                                            .build();

                            mProvider.put(info);
                        });
    }

    /**
     * To be called when Chrome first loads and determines the current update status.  This will
     * determine update success or failure based on previously persisted state and calls to
     * {@link #startUpdate()}.
     */
    public void analyzeFirstStatus() {
        mProvider
                .get()
                .then(
                        state -> {
                            if (state == null) return;

                            // We're using System.currentTimeMillis() here to track time across
                            // restarts.
                            long timedelta = System.currentTimeMillis() - state.getTimestampMs();
                            boolean expired =
                                    timedelta > UpdateConfigs.getUpdateAttributionWindowMs();
                            boolean success =
                                    !TextUtils.equals(
                                            state.getVersion(), VersionConstants.PRODUCT_VERSION);

                            if (success || expired) {
                                mProvider.clear();
                            } else if (!state.getRecordedSession()) {
                                mProvider.put(state.toBuilder().setRecordedSession(true).build());
                            }
                        });
    }
}
