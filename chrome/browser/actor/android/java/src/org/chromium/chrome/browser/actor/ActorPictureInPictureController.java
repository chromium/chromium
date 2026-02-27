// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.actor;

import android.content.res.Configuration;
import android.util.Rational;

import androidx.activity.ComponentActivity;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.core.pip.BasicPictureInPicture;
import androidx.core.pip.PictureInPictureDelegate;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.Supplier;

/**
 * Manages PiP transitions for Actor tasks with absolute priority over video PiP. Uses the AndroidX
 * PiP Jetpack library to handle seamless transitions and OS fragmentation.
 */
@NullMarked
public class ActorPictureInPictureController
        implements ActorKeyedService.Observer,
                PictureInPictureDelegate.OnPictureInPictureEventListener {
    private static final String TAG = "ActorPiPController";
    private final ComponentActivity mActivity;
    private final Supplier<Profile> mProfileSupplier;
    private final BasicPictureInPicture mPipDelegate;
    private @Nullable ActorKeyedService mActorService;
    private boolean mInActorPiP;

    /**
     * @param activity The ComponentActivity.
     * @param profileSupplier The supplier for the current Profile.
     */
    public ActorPictureInPictureController(
            ComponentActivity activity, Supplier<Profile> profileSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        // Initialize the AndroidX PiP delegate.
        // Activity extends ComponentActivity, so this is valid.
        mPipDelegate = new BasicPictureInPicture(activity);
        mPipDelegate.addOnPictureInPictureEventListener(
                ContextCompat.getMainExecutor(activity), this);
        updatePipState();
    }

    /** Checks if there are active Actor tasks. */
    public boolean shouldEnterPip() {
        if (mActivity.isFinishing() || mActivity.isDestroyed()) return false;
        maybeCreateActorService();
        if (mActorService == null) return false;
        return mActorService.getActiveTasksCount() > 0;
    }

    /** Lazily retrieves the ActorKeyedService and registers this observer. */
    private @Nullable ActorKeyedService maybeCreateActorService() {
        if (mActorService != null) return mActorService;
        Profile profile = mProfileSupplier.get();
        if (profile == null) return null;
        mActorService = ActorKeyedServiceFactory.getForProfile(profile);
        if (mActorService != null) {
            mActorService.addObserver(this);
        }
        return mActorService;
    }

    /** Replaces manual PictureInPictureParams building. */
    public void updatePipState() {
        boolean active = shouldEnterPip();
        mPipDelegate.setEnabled(active);
        if (active) {
            mPipDelegate.setAspectRatio(new Rational(16, 9));
        }
    }

    /** For entering PiP via fallback in onUserLeaveHint for older devices. */
    public void attemptPictureInPicture() {
        if (!shouldEnterPip()) return;
        try {
            // The Jetpack delegate has already set up the parameters in updatePipState().
            mActivity.enterPictureInPictureMode();
        } catch (IllegalStateException | IllegalArgumentException e) {
            Log.e(TAG, "Failed to enter PiP mode manually", e);
        }
    }

    // ActorKeyedService.Observer implementation
    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        updatePipState();
        checkAndExitPipIfFinished();
        if (mInActorPiP) {
            updatePipOverlayDetails(taskId);
        }
    }

    private void checkAndExitPipIfFinished() {
        if (mInActorPiP && !shouldEnterPip()) {
            Log.i(TAG, "No active tasks remaining. Exiting PiP.");
            mInActorPiP = false;
            // Standard way to exit PiP programmatically
            mActivity.moveTaskToBack(true);
        }
    }

    private void updatePipOverlayDetails(int taskId) {
        maybeCreateActorService();
        if (mActorService == null) return;
        ActorTask task = mActorService.getTask(taskId);
        if (task == null) return;
        // TODO(crbug.com/484430394): Update status overlay view.
        updatePipState();
    }

    /** Handles when the Activity enters/exits PiP. */
    @Override
    public void onPictureInPictureEvent(
            @NonNull PictureInPictureDelegate.Event event, @Nullable Configuration newConfig) {
        if (event == PictureInPictureDelegate.Event.ENTERED) {
            mInActorPiP = true;
            checkAndExitPipIfFinished();
        } else if (event == PictureInPictureDelegate.Event.EXITED) {
            mInActorPiP = false;
            updatePipState();
        }
    }

    /** Expose to Activity to guarantee UI reset during framework exits. */
    public void onFrameworkExitedPictureInPicture() {
        mInActorPiP = false;
        updatePipState();
    }

    /** Called when the Activity is destroyed. */
    public void destroy() {
        if (mActorService != null) {
            mActorService.removeObserver(this);
            mActorService = null;
        }
        mPipDelegate.setEnabled(false);
    }
}
