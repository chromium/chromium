// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import android.util.Base64;

import org.chromium.base.Promise;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omaha.OmahaBase;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/** A helper class to manage retrieving and storing a persisted instance of {@link Tracking}. */
class TrackingProvider {
    private static final String TRACKING_PERSISTENT_KEY = "UpdateProtos_Tracking";

    private final TaskRunner mTaskRunner;

    /**Builds a new instance of TrackingProvider. */
    public TrackingProvider() {
        mTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT);
    }

    /** @return The persisted instance of {@link Tracking} or {@code null} if none is saved. */
    public Promise<Tracking> get() {
        final Promise<Tracking> promise = new Promise<>();

        mTaskRunner.postTask(() -> {
            Tracking state = null;

            String serialized =
                    OmahaBase.getSharedPreferences().getString(TRACKING_PERSISTENT_KEY, null);
            if (serialized != null) {
                try {
                    state = Tracking.parseFrom(Base64.decode(serialized, Base64.DEFAULT));
                } catch (com.google.protobuf.InvalidProtocolBufferException e) {
                }
            }

            final Tracking finalState = state;
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> promise.fulfill(finalState));
        });

        return promise;
    }

    /** Clears any persisted instance of {@link Tracking}. */
    public void clear() {
        mTaskRunner.postTask(()
                                     -> OmahaBase.getSharedPreferences()
                                                .edit()
                                                .remove(TRACKING_PERSISTENT_KEY)
                                                .apply());
    }

    /**
     * Persists {@code state}, overwriting any currently persisted instance of {@link Tracking}.
     * @param state The new instance of {@link Tracking} to persist.
     */
    public void put(Tracking state) {
        mTaskRunner.postTask(() -> {
            String serialized = Base64.encodeToString(state.toByteArray(), Base64.DEFAULT);
            OmahaBase.getSharedPreferences()
                    .edit()
                    .putString(TRACKING_PERSISTENT_KEY, serialized)
                    .apply();
        });
    }
}
