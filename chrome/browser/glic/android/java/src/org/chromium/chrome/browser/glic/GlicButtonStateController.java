// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;
import java.util.function.Supplier;

/**
 * Common controller for Glic buttons (toolbar and bottom bar). Manages state transitions and
 * observations of Glic services.
 */
@NullMarked
public class GlicButtonStateController
        implements ActorKeyedService.Observer, GlicKeyedService.GlobalShowHideObserver {

    @IntDef({ButtonState.DEFAULT, ButtonState.WORKING, ButtonState.NEEDS_REVIEW, ButtonState.DONE})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface ButtonState {
        int DEFAULT = 0;
        int WORKING = 1;
        int NEEDS_REVIEW = 2;
        int DONE = 3;
    }

    /** Listener for state changes. */
    public interface Listener {
        /** Called when the button state or panel visibility changes. */
        void onStateChanged(@ButtonState int state, boolean isPanelOpen);
    }

    private final Activity mActivity;
    private final Listener mListener;
    private final Supplier<@Nullable ChromeAndroidTask> mTaskSupplier;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    private @Nullable Profile mCurrentProfile;
    private @Nullable ActorKeyedService mCurrentActorService;
    private @Nullable GlicKeyedService mCurrentGlicService;

    private @ButtonState int mButtonState = ButtonState.DEFAULT;
    private boolean mPersistDoneState;
    private boolean mIsPanelOpen;
    private int mBrowserControlsShowingToken = TokenHolder.INVALID_TOKEN;

    /**
     * Constructs a new GlicButtonStateController.
     *
     * @param context The Android context.
     * @param listener The listener for state changes.
     * @param taskSupplier Supplier for the ChromeAndroidTask.
     * @param browserControlsVisibilityManager Manager for browser controls visibility.
     */
    public GlicButtonStateController(
            Activity activity,
            Listener listener,
            Supplier<@Nullable ChromeAndroidTask> taskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager) {
        mActivity = activity;
        mListener = listener;
        mTaskSupplier = taskSupplier;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
    }

    /**
     * Updates observations for the given profile.
     *
     * @param profile The profile to observe.
     */
    public void updateObservations(Profile profile) {
        assert !profile.isOffTheRecord();
        if (profile.equals(mCurrentProfile)) return;

        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.removeGlobalShowHideObserver(this);
        }

        mCurrentProfile = profile;
        mCurrentActorService = ActorKeyedServiceFactory.getForProfile(profile);
        mCurrentGlicService = GlicKeyedServiceFactory.getForProfile(profile);

        if (mCurrentActorService != null) {
            mCurrentActorService.addObserver(this);
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.addGlobalShowHideObserver(this);
            updateIsPanelOpen();
        }
    }

    /** Updates the button state based on the current active task. */
    public void updateButtonState() {
        if (mCurrentActorService == null) {
            updateButtonStateAndControls(ButtonState.DEFAULT);
            return;
        }

        ActorTask task = mCurrentActorService.getCurrentActiveTask();
        int newButtonState;
        if (task != null) {
            newButtonState = mapTaskStateToButtonState(task.getState());
        } else if (mPersistDoneState) {
            newButtonState = ButtonState.DONE;
        } else {
            newButtonState = ButtonState.DEFAULT;
        }

        updateButtonStateAndControls(newButtonState);
    }

    private void updateButtonStateAndControls(int newButtonState) {
        int oldButtonState = mButtonState;
        mButtonState = newButtonState;
        mPersistDoneState = (mButtonState == ButtonState.DONE);

        boolean changed = mButtonState != oldButtonState;

        if (changed) {
            if (mButtonState == ButtonState.WORKING) {
                acquireBrowserControls();
            } else if (oldButtonState == ButtonState.WORKING) {
                releaseBrowserControls();
            }
            mListener.onStateChanged(mButtonState, mIsPanelOpen);
        }
    }

    private void acquireBrowserControls() {
        if (mBrowserControlsShowingToken == TokenHolder.INVALID_TOKEN) {
            mBrowserControlsShowingToken =
                    mBrowserControlsVisibilityManager
                            .getBrowserVisibilityDelegate()
                            .showControlsPersistent();
        }
    }

    private void releaseBrowserControls() {
        if (mBrowserControlsShowingToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsVisibilityManager
                    .getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mBrowserControlsShowingToken);
            mBrowserControlsShowingToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void updateIsPanelOpen() {
        if (mCurrentGlicService == null || mCurrentProfile == null) return;
        ChromeAndroidTask task = mTaskSupplier.get();
        if (task == null) return;

        long browserWindowPtr = task.getNativeBrowserWindowPtr(mCurrentProfile, mActivity);
        boolean isOpen = false;
        if (browserWindowPtr != 0) {
            isOpen = mCurrentGlicService.isPanelShowingForBrowser(browserWindowPtr);
        }
        if (mIsPanelOpen != isOpen) {
            mIsPanelOpen = isOpen;
            mListener.onStateChanged(mButtonState, mIsPanelOpen);
        }
    }

    /**
     * Maps an ActorTaskState to a ButtonState.
     *
     * @param taskState The task state to map.
     * @return The corresponding ButtonState.
     */
    public static @ButtonState int mapTaskStateToButtonState(@ActorTaskState int taskState) {
        switch (taskState) {
            case ActorTaskState.WAITING_ON_USER:
            case ActorTaskState.FAILED:
                return ButtonState.NEEDS_REVIEW;
            case ActorTaskState.FINISHED:
                return ButtonState.DONE;
            case ActorTaskState.ACTING:
            case ActorTaskState.REFLECTING:
                return ButtonState.WORKING;
            case ActorTaskState.CANCELLED:
            case ActorTaskState.CREATED:
            case ActorTaskState.PAUSED_BY_USER:
            case ActorTaskState.PAUSED_BY_ACTOR:
                return ButtonState.DEFAULT;
            default:
                throw new AssertionError("Unexpected task state: " + taskState);
        }
    }

    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        if (newState == ActorTaskState.FINISHED) {
            mPersistDoneState = true;
        }
        updateButtonStateAndControls(mapTaskStateToButtonState(newState));
    }

    @Override
    public void onGlobalShowHide() {
        updateIsPanelOpen();
    }

    /** Returns the current button state. */
    public @ButtonState int getButtonState() {
        return mButtonState;
    }

    /** Returns true if the panel is open. */
    public boolean isPanelOpen() {
        return mIsPanelOpen;
    }

    /**
     * Sets whether to persist the DONE state.
     *
     * @param persist True to persist, false otherwise.
     */
    public void setPersistDoneState(boolean persist) {
        mPersistDoneState = persist;
    }

    /** Returns the list of active tasks. */
    public @Nullable List<ActorTask> getActiveTasks() {
        return mCurrentActorService != null ? mCurrentActorService.getActiveTasks() : null;
    }

    /**
     * Returns the active task ID on the given tab.
     *
     * @param tabId The tab ID to check.
     * @return The active task ID, or null if none.
     */
    public @Nullable Integer getActiveTaskIdOnTab(int tabId) {
        return mCurrentActorService != null
                ? mCurrentActorService.getActiveTaskIdOnTab(tabId)
                : null;
    }

    /** Destroys the controller and removes observers. */
    public void destroy() {
        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
            mCurrentActorService = null;
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.removeGlobalShowHideObserver(this);
            mCurrentGlicService = null;
        }
        mCurrentProfile = null;
        releaseBrowserControls();
    }
}
