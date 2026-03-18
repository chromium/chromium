// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.actor;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.PendingIntent;
import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.drawable.Icon;
import android.util.Rational;
import android.view.ViewGroup;

import androidx.activity.ComponentActivity;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.pip.BasicPictureInPicture;
import androidx.core.pip.PictureInPictureDelegate;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayCoordinator;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;

import java.util.ArrayList;
import java.util.List;
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
    private static final int REQUEST_CODE_PAUSE_RESUME = 101;

    private final ComponentActivity mActivity;
    private final Supplier<Profile> mProfileSupplier;
    private final Supplier<ViewGroup> mRootViewSupplier;
    private final BasicPictureInPicture mPipDelegate;
    private @Nullable ActorKeyedService mActorService;
    private boolean mInActorPiP;
    private @Nullable ActorPictureInPictureOverlayCoordinator mPipOverlayCoordinator;

    /**
     * @param activity The ComponentActivity.
     * @param profileSupplier The supplier for the current Profile.
     * @param rootViewSupplier The supplier for the root view.
     */
    public ActorPictureInPictureController(
            ComponentActivity activity,
            Supplier<Profile> profileSupplier,
            Supplier<ViewGroup> rootViewSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mRootViewSupplier = rootViewSupplier;
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
        ActorKeyedService service = maybeGetActorService();
        if (service == null) return false;
        return service.getActiveTasksCount() > 0;
    }

    /** Lazily retrieves the ActorKeyedService and registers this observer. */
    private @Nullable ActorKeyedService maybeGetActorService() {
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
            updatePausePlayActions();
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
        // TODO(crbug.com/491976823): Store active task ID and clear on task complete.
        updatePipState();
        checkAndExitPipIfFinished();
        if (mInActorPiP && mPipOverlayCoordinator != null) {
            updatePipOverlayStatus(newState);
        }
    }

    private void checkAndExitPipIfFinished() {
        if (!mInActorPiP || shouldEnterPip()) return;

        Log.i(TAG, "No active tasks remaining. Exiting PiP.");
        mInActorPiP = false;
        hideOverlay();
        // Standard way to exit PiP programmatically
        mActivity.moveTaskToBack(true);
    }

    void updatePipOverlayStatus(@ActorTaskState int newState) {
        if (!mInActorPiP || mPipOverlayCoordinator == null) return;

        maybeGetActorService();
        if (mActorService == null) return;
        mPipOverlayCoordinator.updateStatus(newState);
        updatePipState();
    }

    private void updatePausePlayActions() {
        maybeGetActorService();
        if (mActorService == null) return;

        ActorTask task = mActorService.getCurrentActiveTask();
        List<RemoteAction> actions = new ArrayList<>();

        if (task != null) {
            RemoteAction action = createPauseResumeActionForState(task.getId(), task.getState());
            if (action != null) {
                actions.add(action);
            }
        } else {
            mActivity.setPictureInPictureParams(
                    new PictureInPictureParams.Builder().setActions(new ArrayList<>()).build());
            return;
        }

        updatePipActions(actions);
    }

    private void updatePipActions(List<RemoteAction> actions) {
        PictureInPictureParams params =
                new PictureInPictureParams.Builder().setActions(actions).build();
        mActivity.setPictureInPictureParams(params);
    }

    private @Nullable RemoteAction createPauseResumeActionForState(
            @ActorTaskId int taskId, @ActorTaskState int state) {
        boolean isWorking =
                (state == ActorTaskState.CREATED
                        || state == ActorTaskState.ACTING
                        || state == ActorTaskState.REFLECTING);

        boolean isPaused =
                (state == ActorTaskState.PAUSED_BY_ACTOR
                        || state == ActorTaskState.PAUSED_BY_USER
                        || state == ActorTaskState.WAITING_ON_USER);

        if (!isWorking && !isPaused) return null;

        String actionName =
                isPaused
                        ? NotificationConstants.ACTION_ACTOR_RESUME
                        : NotificationConstants.ACTION_ACTOR_PAUSE;

        int iconRes =
                isPaused ? R.drawable.ic_play_arrow_white_24dp : R.drawable.ic_pause_white_24dp;

        String text =
                mActivity.getString(
                        isPaused
                                ? R.string.actor_pip_paused_status
                                : R.string.actor_pip_working_status);

        Intent intent = createIntentForPauseResumeAction(taskId, actionName);

        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(
                        mActivity,
                        REQUEST_CODE_PAUSE_RESUME,
                        intent,
                        PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

        return new RemoteAction(
                Icon.createWithResource(mActivity, iconRes), text, text, pendingIntent);
    }

    /** Handles when the Activity enters/exits PiP. */
    @Override
    public void onPictureInPictureEvent(
            @NonNull PictureInPictureDelegate.Event event, @Nullable Configuration newConfig) {
        if (event == PictureInPictureDelegate.Event.ENTERED) {
            mInActorPiP = true;
            showOverlay();
            checkAndExitPipIfFinished();
        } else if (event == PictureInPictureDelegate.Event.EXITED) {
            mInActorPiP = false;
            hideOverlay();
            updatePipState();
        }
    }

    /** Expose to Activity to guarantee UI reset during framework exits. */
    public void onFrameworkExitedPictureInPicture() {
        mInActorPiP = false;
        hideOverlay();
        updatePipState();
    }

    /** Called when the Activity is destroyed. */
    public void destroy() {
        if (mPipOverlayCoordinator != null) {
            mPipOverlayCoordinator.destroy();
            mPipOverlayCoordinator = null;
        }

        if (mActorService != null) {
            mActorService.removeObserver(this);
            mActorService = null;
        }
        mPipDelegate.setEnabled(false);
    }

    @VisibleForTesting
    Intent createIntentForPauseResumeAction(@ActorTaskId int taskId, String actionName) {
        Intent intent = new Intent(actionName).setPackage(mActivity.getPackageName());

        Profile profile = mProfileSupplier.get();
        if (profile != null) {
            ProfileIntentUtils.addProfileToIntent(profile, intent);
        }

        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        return intent;
    }

    private void showOverlay() {
        if (mPipOverlayCoordinator == null) {
            ViewGroup parent = mRootViewSupplier.get();
            if (parent == null) return;
            mPipOverlayCoordinator = new ActorPictureInPictureOverlayCoordinator(mActivity, parent);
        }

        assumeNonNull(mPipOverlayCoordinator);
        mPipOverlayCoordinator.setVisibility(true);

        ActorKeyedService service = maybeGetActorService();
        if (service != null) {
            ActorTask task = service.getCurrentActiveTask();
            if (task != null) {
                mPipOverlayCoordinator.updateTitle(task.getTitle());
                updatePipOverlayStatus(task.getState());
            }
        }
    }

    private void hideOverlay() {
        if (mPipOverlayCoordinator != null) {
            mPipOverlayCoordinator.setVisibility(false);
        }
    }

    void setOverlayCoordinatorForTesting(ActorPictureInPictureOverlayCoordinator coordinator) {
        mPipOverlayCoordinator = coordinator;
    }
}
