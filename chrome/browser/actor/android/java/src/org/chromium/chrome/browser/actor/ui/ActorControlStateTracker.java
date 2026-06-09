// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskId;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.glic.GlicInstanceHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;

import java.util.Set;

/** Tracks the state of Actor tasks and GLIC instances to determine PeekView UI state. */
@NullMarked
public class ActorControlStateTracker
        implements ActorKeyedService.Observer, GlicInstanceHelper.Observer {

    private static final String TAG = "ActorControlTracker";

    /** Observer for state changes. */
    public interface Observer {
        /** Called when the underlying task or conversation state changes. */
        void onStateChanged();
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Callback<Profile> mProfileObserver;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final TabSupplierObserver mTabObserver;

    private @Nullable ActorKeyedService mActorKeyedService;
    private @Nullable GlicInstanceHelper mGlicInstanceHelper;

    private String mActiveTaskTitle = "";

    // The latest known state of the active task. This is tracked locally so that the state
    // remains available even if the active task object is cleared upon task completion.
    private @Nullable @ActorTaskState Integer mLatestTaskState;

    // The ID of the tab that Actor was last acting on. This is used to switch to the actuating tab
    // when the user clicks the actor control button in the WAITING state.
    private int mActuatedTabId = Tab.INVALID_TAB_ID;

    // The conversation ID that the active task is associated with. This is used to determine
    // whether the active task is associated with the current conversation or not.
    private String mTaskGlicConversationId = "";

    // The ID of the currently active GLIC instance.
    private String mActiveGlicConversationId = "";

    private String mConversationTitle = "";

    /**
     * Constructs a new {@link ActorControlStateTracker}.
     *
     * @param profileSupplier The {@link MonotonicObservableSupplier} for the profile.
     * @param tabSupplier The {@link NullableObservableSupplier<Tab>} for the tab. This supplies the
     *     currently active tab in the current activity. It provides the active tab regardless of
     *     mode (regular or incognito), updating when the user switches modes or tabs.
     */
    public ActorControlStateTracker(
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> tabSupplier) {
        mProfileSupplier = profileSupplier;

        mProfileObserver = this::onProfileAdded;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);

        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mGlicInstanceHelper != null) {
                            mGlicInstanceHelper.removeObserver(ActorControlStateTracker.this);
                        }
                        if (tab != null && tab.isOffTheRecord()) {
                            resetGlicInstanceState();
                            resetTaskState();
                            updateState();
                            return;
                        }
                        mGlicInstanceHelper = tab != null ? GlicInstanceHelper.from(tab) : null;
                        if (mGlicInstanceHelper != null) {
                            mGlicInstanceHelper.addObserver(ActorControlStateTracker.this);
                        }
                        onInstanceChanged();
                    }
                };
    }

    /** Adds an observer to be notified of state changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
        observer.onStateChanged();
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    private void onProfileAdded(Profile profile) {
        if (mActorKeyedService != null) {
            mActorKeyedService.removeObserver(this);
            mActorKeyedService = null;
        }
        boolean isProfileValid =
                profile != null && profile.isNativeInitialized() && !profile.isOffTheRecord();
        if (!isProfileValid) {
            resetTaskState();
            resetGlicInstanceState();
            updateState();
            return;
        }

        mActorKeyedService = ActorKeyedServiceFactory.getForProfile(profile);
        if (mActorKeyedService != null) {
            mActorKeyedService.addObserver(this);
        }
        ActorTask activeTask =
                mActorKeyedService != null ? mActorKeyedService.getCurrentActiveTask() : null;
        if (activeTask == null) {
            mLatestTaskState = null;
            updateState();
        } else {
            onTaskStateChanged(activeTask.getId(), activeTask.getState());
        }
    }

    private void updateState() {
        for (Observer observer : mObservers) {
            observer.onStateChanged();
        }
    }

    /**
     * Called when the state of the task changes.
     *
     * @param taskId The ID of the task that changed.
     * @param newState The new state of the task.
     */
    @Override
    public void onTaskStateChanged(@ActorTaskId int taskId, @ActorTaskState int newState) {
        if (mActorKeyedService == null) {
            return;
        }

        mLatestTaskState = newState;

        ActorTask activeTask = mActorKeyedService.getCurrentActiveTask();
        // TODO(crbug.com/503370476): Use ActorUiStateManager to track when tasks are finished,
        // instead of checking activeTask and the newState.
        if (activeTask != null) {
            mActiveTaskTitle = activeTask.getTitle();
            mTaskGlicConversationId =
                    TextUtils.isEmpty(mTaskGlicConversationId)
                            ? mActiveGlicConversationId
                            : mTaskGlicConversationId;

            Set<Integer> tabs = activeTask.getLastActedTabs();
            mActuatedTabId = tabs.isEmpty() ? Tab.INVALID_TAB_ID : tabs.iterator().next();
        } else if (!ActorUtils.isCompletedState(newState)) {
            // If the active task is null but the task has not been completed, we are in an invalid
            // state. Clear PeekView content and reset task-related variables.
            mActiveTaskTitle = "";
            mTaskGlicConversationId = "";
            mActuatedTabId = Tab.INVALID_TAB_ID;
            Log.w(
                    TAG,
                    "Active task is null but task has not been completed. newState: %d",
                    newState);
        }

        updateState();

        // Clean up state only after the UI has been updated to reflect task completion.
        if (ActorUtils.isCompletedState(newState)) {
            mActiveTaskTitle = "";
            mTaskGlicConversationId = "";
        }
    }

    /** Called when the GLIC instance changes. */
    @Override
    public void onInstanceChanged() {
        if (mGlicInstanceHelper == null) {
            resetGlicInstanceState();
            updateState();
            return;
        }

        mConversationTitle = mGlicInstanceHelper.getConversationTitle();
        mActiveGlicConversationId = mGlicInstanceHelper.getConversationId();
        updateState();
    }

    /** Cleans up observers and suppliers. */
    public void destroy() {
        if (mActorKeyedService != null) {
            mActorKeyedService.removeObserver(this);
        }
        if (mGlicInstanceHelper != null) {
            mGlicInstanceHelper.removeObserver(this);
        }
        if (mTabObserver != null) {
            mTabObserver.destroy();
        }
        if (mProfileSupplier != null && mProfileObserver != null) {
            mProfileSupplier.removeObserver(mProfileObserver);
        }
    }

    /** Clears the completed task state after the user acknowledges it. */
    public void clearWaitingState() {
        mLatestTaskState = null;
        mActuatedTabId = Tab.INVALID_TAB_ID;
        updateState();
    }

    /** Returns the currently active task, or null. */
    public @Nullable ActorTask getCurrentActiveTask() {
        return mActorKeyedService != null ? mActorKeyedService.getCurrentActiveTask() : null;
    }

    /** Returns the ID of the tab that Actor was last acting on. */
    public int getActuatedTabId() {
        return mActuatedTabId;
    }

    /** Returns the title of the active task. */
    public String getActiveTaskTitle() {
        return mActiveTaskTitle;
    }

    /** Returns the title of the current conversation. */
    public String getConversationTitle() {
        return mConversationTitle;
    }

    /** Returns whether the active task belongs to the currently active conversation. */
    public boolean isActiveTaskAssociatedWithConversation() {
        return mActiveGlicConversationId.equals(mTaskGlicConversationId);
    }

    /** Returns the latest known state of the active task, or null if no task active. */
    public @Nullable @ActorTaskState Integer getLatestTaskState() {
        return mLatestTaskState;
    }

    private void resetGlicInstanceState() {
        mConversationTitle = "";
        mActiveGlicConversationId = "";
    }

    private void resetTaskState() {
        mLatestTaskState = null;
        mActiveTaskTitle = "";
        mTaskGlicConversationId = "";
        mActuatedTabId = Tab.INVALID_TAB_ID;
    }
}
