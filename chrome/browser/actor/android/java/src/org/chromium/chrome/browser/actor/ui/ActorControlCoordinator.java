// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.Log;
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
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/** The Coordinator for the Actor Control component. */
@NullMarked
public class ActorControlCoordinator
        implements ActorKeyedService.Observer, GlicInstanceHelper.Observer {

    /** Delegate for handling tab selection. */
    @FunctionalInterface
    public interface TabSelectionDelegate {
        void switchToTab(int tabId);
    }

    private static final String TAG = "ActorControlCoordin";

    private final ActorControlMediator mMediator;
    private final PropertyModel mModel;
    private final Callback<Profile> mProfileObserver;
    private final TabBottomSheetManager mTabBottomSheetManager;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final TabSupplierObserver mTabObserver;
    private final TabSelectionDelegate mTabSelectionDelegate;

    private @Nullable ActorKeyedService mActorKeyedService;
    private @Nullable GlicInstanceHelper mGlicInstanceHelper;

    private String mActiveTaskTitle = "";
    private String mConversationTitle = "";
    private PeekViewUiState mPeekViewUiState = PeekViewUiState.DEFAULT;

    // The ID of the currently active GLIC instance.
    private String mActiveGlicConversationId = "";

    // The conversation ID that the active task is associated with. This is used to determine
    // whether the active task is associated with the current conversation or not.
    private String mTaskGlicConversationId = "";

    // The ID of the tab that Actor was last acting on. This is used to switch to the actuating tab
    // when the user clicks the actor control button in the WAITING state.
    private int mActuatedTabId = Tab.INVALID_TAB_ID;

    /**
     * Constructs a new {@link ActorControlCoordinator}.
     *
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for the tab bottom sheet.
     * @param profileSupplier The {@link ObservableSupplier<Profile>} for the profile.
     * @param tabSupplier The {@link NullableObservableSupplier<Tab>} for the tab. This supplies the
     *     currently active tab in the current activity. It provides the active tab regardless of
     *     mode (regular or incognito), updating when the user switches modes or tabs.
     * @param tabSelectionDelegate The delegate to handle tab selection.
     */
    // TODO(crbug.com/491895203): Add render test for peek view.
    public ActorControlCoordinator(
            TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            TabSelectionDelegate tabSelectionDelegate) {
        mTabBottomSheetManager = tabBottomSheetManager;
        mProfileSupplier = profileSupplier;
        mTabSelectionDelegate = tabSelectionDelegate;

        mModel =
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(TabBottomSheetPeekProperties.TITLE_TEXT, "")
                        .with(
                                TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED,
                                this::onActorControlClicked)
                        .with(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED, this::onCloseClicked)
                        .with(
                                TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED,
                                this::onPeekViewClicked)
                        .build();

        mMediator = new ActorControlMediator(mModel);

        mProfileObserver = this::onProfileAdded;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);

        setPeekViewContent("", PeekViewUiState.DEFAULT);
        mTabBottomSheetManager.setPeekViewModel(mModel);

        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mGlicInstanceHelper != null) {
                            mGlicInstanceHelper.removeObserver(ActorControlCoordinator.this);
                        }
                        if (tab != null && tab.isOffTheRecord()) {
                            mGlicInstanceHelper = null;
                            clearPeekViewContent();
                            return;
                        }
                        mGlicInstanceHelper = tab != null ? GlicInstanceHelper.from(tab) : null;
                        if (mGlicInstanceHelper != null) {
                            mGlicInstanceHelper.addObserver(ActorControlCoordinator.this);
                        }
                        onInstanceChanged();
                    }
                };
    }

    private void onProfileAdded(Profile profile) {
        if (mActorKeyedService != null) {
            mActorKeyedService.removeObserver(this);
            mActorKeyedService = null;
        }
        boolean isProfileValid =
                profile != null && profile.isNativeInitialized() && !profile.isOffTheRecord();
        if (!isProfileValid) {
            clearPeekViewContent();
            return;
        }

        mActorKeyedService = ActorKeyedServiceFactory.getForProfile(profile);
        if (mActorKeyedService != null) {
            mActorKeyedService.addObserver(this);
        }
        ActorTask activeTask =
                mActorKeyedService != null ? mActorKeyedService.getCurrentActiveTask() : null;
        if (activeTask != null) {
            onTaskStateChanged(activeTask.getId(), activeTask.getState());
        }
    }

    private void setPeekViewContent(String title, PeekViewUiState state) {
        mPeekViewUiState = state;
        mMediator.setContent(title, state);
    }

    private void clearPeekViewContent() {
        mPeekViewUiState = PeekViewUiState.DEFAULT;
        mMediator.setContent("", PeekViewUiState.DEFAULT);
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

        updatePeekView(newState);

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
            clearPeekViewContent();
            return;
        }

        mConversationTitle = mGlicInstanceHelper.getConversationTitle();
        mActiveGlicConversationId = mGlicInstanceHelper.getConversationId();

        ActorTask activeTask =
                mActorKeyedService != null ? mActorKeyedService.getCurrentActiveTask() : null;
        updatePeekView(activeTask != null ? activeTask.getState() : null);
    }

    /**
     * Updates the peek view content based on the current state.
     *
     * @param taskState The state of the task to consider.
     */
    private void updatePeekView(@Nullable @ActorTaskState Integer taskState) {
        if (mActiveGlicConversationId.equals(mTaskGlicConversationId)
                && !TextUtils.isEmpty(mActiveTaskTitle)
                && taskState != null) {
            switch (taskState) {
                case ActorTaskState.CREATED:
                case ActorTaskState.CANCELLED:
                    setPeekViewContent(mActiveTaskTitle, PeekViewUiState.DEFAULT);
                    break;
                case ActorTaskState.ACTING:
                case ActorTaskState.REFLECTING:
                    setPeekViewContent(mActiveTaskTitle, PeekViewUiState.ACTING);
                    break;
                case ActorTaskState.PAUSED_BY_USER:
                    setPeekViewContent(mActiveTaskTitle, PeekViewUiState.PAUSED);
                    break;
                case ActorTaskState.PAUSED_BY_ACTOR:
                case ActorTaskState.WAITING_ON_USER:
                case ActorTaskState.FINISHED:
                case ActorTaskState.FAILED:
                    setPeekViewContent(mActiveTaskTitle, PeekViewUiState.WAITING);
                    break;
                default:
                    assert false : "Unhandled ActorTaskState " + taskState;
                    clearPeekViewContent();
                    break;
            }
        } else {
            setPeekViewContent(mConversationTitle, PeekViewUiState.DEFAULT);
        }
    }

    /** Cleans up component */
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
        mTabBottomSheetManager.removePeekViewModel();
    }

    /** Called when the actor control button is clicked. */
    /* package */ void onActorControlClicked() {
        assert mActorKeyedService != null;

        // TODO(crbug.com/503370476): Use ActorUiStateManager to track when tasks are finished,
        // instead of checking activeTask.
        ActorTask activeTask = mActorKeyedService.getCurrentActiveTask();
        if (activeTask == null) {
            // When a task finishes, PeekView transitions to the "WAITING" state. It will remain in
            // this state until the user dismisses it, so it is possible for a user to press the
            // actor control button when there is no active task.
            if (PeekViewUiState.WAITING.equals(mPeekViewUiState)) {
                // In the WAITING state, the actor control button is the "View" button.
                if (mActuatedTabId != Tab.INVALID_TAB_ID) {
                    mTabSelectionDelegate.switchToTab(mActuatedTabId);
                    mActuatedTabId = Tab.INVALID_TAB_ID;
                }
                mTabBottomSheetManager.setSheetExpanded(true);
                setPeekViewContent(mConversationTitle, PeekViewUiState.DEFAULT);
            } else {
                Log.w(TAG, "onActorControlClicked: No active task and not in WAITING state.");
                clearPeekViewContent();
            }
            return;
        }

        @ActorTaskState int currentState = activeTask.getState();
        switch (currentState) {
            case ActorTaskState.ACTING:
            case ActorTaskState.REFLECTING:
                activeTask.pause();
                break;
            case ActorTaskState.PAUSED_BY_USER:
                activeTask.resume();
                break;
            case ActorTaskState.PAUSED_BY_ACTOR:
            case ActorTaskState.WAITING_ON_USER:
                mTabBottomSheetManager.setSheetExpanded(true);
                break;
            default:
                Log.w(
                        TAG,
                        "onActorControlClicked: Unhandled state %d for task %d",
                        currentState,
                        activeTask.getId());
                break;
        }
    }

    /** Called when the close button is clicked. */
    /* package */ void onCloseClicked() {
        assert mTabBottomSheetManager.isSheetInitialized();
        GlicMetrics.recordClosePeekView();
        mTabBottomSheetManager.tryToCloseBottomSheet(/* animate= */ true);
    }

    /** Called when the peek view is clicked. */
    /* package */ void onPeekViewClicked() {
        mTabBottomSheetManager.setSheetExpanded(true);
    }

    /**
     * @return The {@link PropertyModel} for this component.
     */
    /* package */ PropertyModel getModelForTesting() {
        return mModel;
    }

    /**
     * @return The {@link ActorControlMediator} for this component.
     */
    /* package */ ActorControlMediator getMediatorForTesting() {
        return mMediator;
    }

    /* package */ PeekViewUiState getPeekViewUiStateForTesting() {
        return mPeekViewUiState;
    }

    /* package */ void setPeekViewContentForTesting(String title, PeekViewUiState state) {
        setPeekViewContent(title, state);
    }
}
