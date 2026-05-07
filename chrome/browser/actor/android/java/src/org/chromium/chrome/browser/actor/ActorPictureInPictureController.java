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
import android.os.Handler;
import android.os.Looper;
import android.util.Rational;
import android.util.Size;
import android.view.ViewGroup;

import androidx.activity.ComponentActivity;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.pip.BasicPictureInPicture;
import androidx.core.pip.PictureInPictureDelegate;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayCoordinator;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;
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
    private static final long PIP_EXIT_DELAY_MS = TimeUnit.MINUTES.toMillis(1);

    private final ComponentActivity mActivity;
    private final Supplier<@Nullable Profile> mProfileSupplier;
    private final Supplier<@Nullable ViewGroup> mRootViewSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final Runnable mHideTabSwitcherCallback;
    private final Callback<Boolean> mToggleGlicCallback;
    private final BasicPictureInPicture mPipDelegate;
    private final Size mLastNormalSizeBeforePip;
    private final Callback<Boolean> mOnPipChangedCallback;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private @Nullable ActorKeyedService mActorService;
    private boolean mInActorPiP;
    private @Nullable ActorPictureInPictureOverlayCoordinator mPipOverlayCoordinator;
    private @Nullable Runnable mExitPipRunnable;
    private @Nullable Runnable mTabSelectRunnable;
    private @Nullable Tab mActingTab;
    private boolean mReceivedNewIntent;

    /**
     * @param activity The ComponentActivity.
     * @param profileSupplier The supplier for the current Profile.
     * @param rootViewSupplier The supplier for the root view.
     * @param tabModelSelectorSupplier The supplier for the TabModelSelector.
     * @param hideTabSwitcherCallback Callback to exit the tab switcher.
     * @param toggleGlicCallback Callback to toggle Glic UI.
     * @param lastNormalSizeBeforePip The size of the activity before entering PiP.
     * @param onPipChangedCallback Callback to notify when PiP mode changes.
     */
    public ActorPictureInPictureController(
            ComponentActivity activity,
            Supplier<@Nullable Profile> profileSupplier,
            Supplier<@Nullable ViewGroup> rootViewSupplier,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            Runnable hideTabSwitcherCallback,
            Callback<Boolean> toggleGlicCallback,
            Size lastNormalSizeBeforePip,
            Callback<Boolean> onPipChangedCallback) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mRootViewSupplier = rootViewSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mHideTabSwitcherCallback = hideTabSwitcherCallback;
        mToggleGlicCallback = toggleGlicCallback;
        mLastNormalSizeBeforePip = lastNormalSizeBeforePip;
        mOnPipChangedCallback = onPipChangedCallback;
        // Initialize the AndroidX PiP delegate.
        // Activity extends ComponentActivity, so this is valid.
        mPipDelegate = new BasicPictureInPicture(activity);
        mPipDelegate.addOnPictureInPictureEventListener(
                ContextCompat.getMainExecutor(activity), this);
        updatePipState();
    }

    /**
     * Called when the Activity receives a new intent.
     *
     * <p>ActorPictureInPictureController will automatically perform a tab routing attempt when PiP
     * exits. In cases where PiP exits due to non-actor initiated intent (e.g. VIEW intent), we
     * should not perform the tab routing.
     *
     * @param intent The new intent.
     */
    public void onNewIntent(Intent intent) {
        boolean isActorIntent =
                intent.getBooleanExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false);

        // We only record mReceivedNewIntent when actor is in pip and we get intent from somewhere
        // that is not for actor, so in this case we shouldn't open tab.
        if (mInActorPiP && !isActorIntent) {
            mReceivedNewIntent = true;
        }
    }

    /** Checks if there are active Actor tasks. */
    public boolean shouldEnterPip() {
        if (mActivity.isFinishing() || mActivity.isDestroyed()) return false;
        ActorKeyedService service = maybeGetActorService();
        if (service == null) return false;
        return service.getActiveTasksCount() > 0;
    }

    /** Returns the current tab being acted upon by the active Actor task. */
    public @Nullable Tab getCurrentActingTab() {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return null;

        int tabId = getActiveTaskLastActedTabId();
        return (tabId != Tab.INVALID_TAB_ID) ? selector.getTabById(tabId) : null;
    }

    /**
     * Returns the ID of a tab recently acted upon by the current active task. Currently, Actor
     * tasks are limited to a single tab per action, so this method returns the only available ID.
     * If multiple tabs are present, an arbitrary ID from the set is returned.
     */
    public int getActiveTaskLastActedTabId() {
        // TODO(crbug.com/496226553): Update this to identify the specific active tab
        // if ActorTask begins supporting multi-tab actions.
        maybeGetActorService();
        if (mActorService == null) return Tab.INVALID_TAB_ID;

        ActorTask task = mActorService.getCurrentActiveTask();
        if (task == null) return Tab.INVALID_TAB_ID;

        Set<Integer> tabIds = task.getLastActedTabs();

        // Assume that there is only one tab ID in the set returned by task.getLastActedTabs().
        return !tabIds.isEmpty() ? tabIds.iterator().next() : Tab.INVALID_TAB_ID;
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

        ActorKeyedService service = maybeGetActorService();
        ActorTask task = (service != null) ? service.getTask(taskId) : null;

        if (task != null && task.isCompleted()) {
            stopOffscreenRendering();
            checkAndExitPipIfFinished();
        } else if (shouldEnterPip()) {
            cancelPendingExit();
        }

        if (mInActorPiP && mPipOverlayCoordinator != null) {
            updatePipOverlayStatus(newState);
        }
    }

    private void checkAndExitPipIfFinished() {
        if (!mInActorPiP) return;

        if (shouldEnterPip()) {
            cancelPendingExit();
            return;
        }

        if (mExitPipRunnable != null) return;

        Log.i(TAG, "No active tasks remaining. Scheduling PiP exit in 1 hour.");
        mExitPipRunnable =
                () -> {
                    mExitPipRunnable = null;
                    if (mInActorPiP && !shouldEnterPip()) {
                        Log.i(TAG, "Exiting PiP after 1 hour delay.");
                        mInActorPiP = false;
                        hideOverlay();
                        mActivity.moveTaskToBack(true);
                        ActorMetrics.recordPipStatus(ActorMetrics.ActorPipStatus.EXITED);
                    }
                };
        mHandler.postDelayed(mExitPipRunnable, PIP_EXIT_DELAY_MS);
    }

    private void cancelPendingExit() {
        if (mExitPipRunnable != null) {
            mHandler.removeCallbacks(mExitPipRunnable);
            mExitPipRunnable = null;
        }
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
            PictureInPictureDelegate.Event event, @Nullable Configuration newConfig) {
        if (event == PictureInPictureDelegate.Event.ENTERED) {
            enterPictureInPicture();
        } else if (event == PictureInPictureDelegate.Event.EXITED) {
            exitPictureInPicture();
        }
    }

    /** Expose to Activity to guarantee UI reset during framework exits. */
    public void onFrameworkExitedPictureInPicture() {
        exitPictureInPicture();
    }

    private void enterPictureInPicture() {
        if (mTabSelectRunnable != null) {
            mHandler.removeCallbacks(mTabSelectRunnable);
            mTabSelectRunnable = null;
        }
        mInActorPiP = true;
        mReceivedNewIntent = false;
        mActingTab = getCurrentActingTab();
        ActorMetrics.recordPipStatus(ActorMetrics.ActorPipStatus.ENTERED);
        startOffscreenRendering();
        showOverlay();
        checkAndExitPipIfFinished();
    }

    private void exitPictureInPicture() {
        if (!mInActorPiP) return;

        mInActorPiP = false;
        ActorMetrics.recordPipStatus(ActorMetrics.ActorPipStatus.EXITED);
        stopOffscreenRendering();

        // Delay the tab selection to ensure it runs after onNewIntent has been processed.
        // This allows us to detect if the expansion was caused by a new intent (e.g. launcher icon
        // or external link) vs a manual expand button click.
        mTabSelectRunnable =
                () -> {
                    mTabSelectRunnable = null;
                    maybeSelectActingTabOnExpand();
                };
        mHandler.post(mTabSelectRunnable);

        hideOverlay();
        updatePipState();
        cancelPendingExit();
    }

    private void maybeSelectActingTabOnExpand() {
        // If the activity was resumed with a new intent that is not an Actor intent (e.g. launcher
        // icon, external link), we should not select the acting tab. This allows the normal
        // Chrome intent handling to take over.
        if (mReceivedNewIntent) {
            mReceivedNewIntent = false;
            return;
        }

        ActorMetrics.recordPipUserInteraction(ActorMetrics.ActorPipUserInteraction.EXPAND);
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        int tabId = getActiveTaskLastActedTabId();
        Tab tab = (tabId != Tab.INVALID_TAB_ID) ? selector.getTabById(tabId) : null;

        if (tab != null) {
            mHideTabSwitcherCallback.run();
            selector.selectModel(tab.isIncognitoBranded());
            TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);

            mToggleGlicCallback.onResult(true);
        }
    }

    private void startOffscreenRendering() {
        if (mActingTab == null || mActingTab.getWebContents() == null) return;

        if (mOnPipChangedCallback != null) mOnPipChangedCallback.onResult(true);
        OffscreenRenderingManager.getInstance()
                .startOffscreenRendering(
                        mActingTab,
                        mLastNormalSizeBeforePip.getWidth(),
                        mLastNormalSizeBeforePip.getHeight());
    }

    private void stopOffscreenRendering() {
        if (mActingTab == null || mActingTab.getWebContents() == null) return;

        OffscreenRenderingManager.getInstance().stopOffscreenRendering(mActingTab);
        if (mOnPipChangedCallback != null) mOnPipChangedCallback.onResult(false);

        mActingTab = null;
    }

    /** Called when the Activity is destroyed. */
    public void destroy() {
        cancelPendingExit();
        if (mTabSelectRunnable != null) {
            mHandler.removeCallbacks(mTabSelectRunnable);
            mTabSelectRunnable = null;
        }
        if (mPipOverlayCoordinator != null) {
            mPipOverlayCoordinator.destroy();
            mPipOverlayCoordinator = null;
        }

        if (mActorService != null) {
            for (ActorTask task : mActorService.getActiveTasks()) {
                mActorService.stopTask(task.getId(), StoppedReason.STOPPED_BY_USER);
            }
            mActorService.removeObserver(this);
            mActorService = null;
        }
        mPipDelegate.setEnabled(false);
        OffscreenRenderingManager.getInstance().destroy();
    }

    @VisibleForTesting
    Intent createIntentForPauseResumeAction(@ActorTaskId int taskId, String actionName) {
        Intent intent = new Intent(actionName).setPackage(mActivity.getPackageName());

        Profile profile = mProfileSupplier.get();
        if (profile != null) {
            ProfileIntentUtils.addProfileToIntent(profile, intent);
        }

        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(
                NotificationConstants.EXTRA_ACTOR_PAUSE_RESUME_SOURCE,
                ActorMetrics.ActorPauseResumeSource.PIP);
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

    public void setInActorPiPForTesting(boolean inPiP) {
        mInActorPiP = inPiP;
    }
}
