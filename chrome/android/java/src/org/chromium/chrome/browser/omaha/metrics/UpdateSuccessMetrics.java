// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omaha.UpdateConfigs;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateInteractionSource;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;
import org.chromium.components.version_info.VersionConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A helper class for tracking whether or not an update was successful.  This tracker works across
 * restarts, as update success cannot be immediately determined.
 */
public class UpdateSuccessMetrics {
    /** The type of update currently running.  Used for identifying which metric to tag. */
    @IntDef({UpdateType.INTENT, UpdateType.INLINE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateType {
        /** The update is using the intent mechanism. */
        int INTENT = 0;

        /** The update is using the inline mechanism. */
        int INLINE = 1;
    }

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

    /**
     * To be called right before we are about to interact with the Play Store for an update.
     * @param type   The type of update (see {@link #UpdateType}).
     * @param source The source of the update (see {@link
     *         UpdateStatusProvider#UpdateInteractionSource}).
     */
    public void startUpdate(@UpdateType int type, @UpdateInteractionSource int source) {
        mProvider.get().then(state -> {
            HistogramUtils.recordStartedUpdateHistogram(state != null);

            // We're using System.currentTimeMillis() here to track time across restarts.
            Tracking info = Tracking.newBuilder()
                                    .setTimestampMs(System.currentTimeMillis())
                                    .setVersion(VersionConstants.PRODUCT_VERSION)
                                    .setType(getProtoType(type))
                                    .setSource(getProtoSource(source))
                                    .setRecordedSession(false)
                                    .build();

            mProvider.put(info);
        });
    }

    /**
     * To be called when Chrome first loads and determines the current update status.  This will
     * determine update success or failure based on previously persisted state and calls to
     * {@link #startUpdate(int, int)}.
     * @param status The current {@link UpdateStatus}.
     */
    public void analyzeFirstStatus(UpdateStatus status) {
        if (isUpdateInProgress(status.updateState)) return;

        mProvider.get().then(state -> {
            if (state == null) return;

            // We're using System.currentTimeMillis() here to track time across restarts.
            long timedelta = System.currentTimeMillis() - state.getTimestampMs();
            boolean expired = timedelta > UpdateConfigs.getUpdateAttributionWindowMs();
            boolean success =
                    !TextUtils.equals(state.getVersion(), VersionConstants.PRODUCT_VERSION);

            if (!state.getRecordedSession()) {
                HistogramUtils.recordResultHistogram(AttributionType.SESSION, state, success);
            }

            if (success || expired) {
                HistogramUtils.recordResultHistogram(AttributionType.TIME_WINDOW, state, success);
            }

            if (success || expired) {
                mProvider.clear();
            } else if (!state.getRecordedSession()) {
                mProvider.put(state.toBuilder().setRecordedSession(true).build());
            }
        });
    }

    private static boolean isUpdateInProgress(@UpdateState int state) {
        switch (state) {
            case UpdateState.INLINE_UPDATE_DOWNLOADING:
            case UpdateState.INLINE_UPDATE_READY: // Intentional fallthrough.
                return true;
            default:
                return false;
        }
    }

    private static Type getProtoType(@UpdateType int type) {
        switch (type) {
            case UpdateType.INTENT:
                return Type.INTENT;
            case UpdateType.INLINE:
                return Type.INLINE;
            default:
                return Type.UNKNOWN_TYPE;
        }
    }

    private static Source getProtoSource(@UpdateInteractionSource int source) {
        switch (source) {
            case UpdateInteractionSource.FROM_MENU:
                return Source.FROM_MENU;
            case UpdateInteractionSource.FROM_INFOBAR:
                return Source.FROM_INFOBAR;
            case UpdateInteractionSource.FROM_NOTIFICATION:
                return Source.FROM_NOTIFICATION;
            default:
                return Source.UNKNOWN_SOURCE;
        }
    }
}
