// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;


import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** The Coordinator for the Actor Control component. */
@NullMarked
public class ActorControlCoordinator implements ActorControlStateTracker.Observer {

    /** Delegate for handling tab selection. */
    @FunctionalInterface
    public interface TabSelectionDelegate {
        void switchToTab(int tabId);
    }

    private static final String TAG = "ActorControlCoordin";

    private final ActorControlMediator mMediator;
    private final PropertyModel mModel;
    private final TabBottomSheetManager mTabBottomSheetManager;
    private final TabSelectionDelegate mTabSelectionDelegate;
    private final ActorControlStateTracker mStateTracker;
    private PeekViewUiState mPeekViewUiState = PeekViewUiState.DEFAULT;

    /**
     * Constructs a new {@link ActorControlCoordinator}.
     *
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for the tab bottom sheet.
     * @param stateTracker The tracker for the UI state.
     * @param tabSelectionDelegate The delegate to handle tab selection.
     */
    // TODO(crbug.com/491895203): Add render test for peek view.
    public ActorControlCoordinator(
            TabBottomSheetManager tabBottomSheetManager,
            ActorControlStateTracker stateTracker,
            TabSelectionDelegate tabSelectionDelegate) {
        mTabBottomSheetManager = tabBottomSheetManager;
        mTabSelectionDelegate = tabSelectionDelegate;
        mStateTracker = stateTracker;

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

        mTabBottomSheetManager.setPeekViewModel(mModel);
        mStateTracker.addObserver(this);
    }

    @Override
    public void onStateChanged() {
        updatePeekView();
    }

    private void updatePeekView() {
        @ActorTaskState Integer taskState = mStateTracker.getLatestTaskState();
        String activeTaskTitle = mStateTracker.getActiveTaskTitle();
        String conversationTitle = mStateTracker.getConversationTitle();
        boolean isAssociated = mStateTracker.isActiveTaskAssociatedWithConversation();

        if (isAssociated && !TextUtils.isEmpty(activeTaskTitle) && taskState != null) {
            switch (taskState) {
                case ActorTaskState.CREATED:
                case ActorTaskState.CANCELLED:
                    setPeekViewContent(activeTaskTitle, PeekViewUiState.DEFAULT);
                    break;
                case ActorTaskState.ACTING:
                case ActorTaskState.REFLECTING:
                    setPeekViewContent(activeTaskTitle, PeekViewUiState.ACTING);
                    break;
                case ActorTaskState.PAUSED_BY_USER:
                    setPeekViewContent(activeTaskTitle, PeekViewUiState.PAUSED);
                    break;
                case ActorTaskState.PAUSED_BY_ACTOR:
                case ActorTaskState.WAITING_ON_USER:
                case ActorTaskState.FINISHED:
                case ActorTaskState.FAILED:
                    setPeekViewContent(activeTaskTitle, PeekViewUiState.WAITING);
                    break;
                default:
                    assert false : "Unhandled ActorTaskState " + taskState;
                    clearPeekViewContent();
                    break;
            }
        } else {
            setPeekViewContent(conversationTitle, PeekViewUiState.DEFAULT);
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

    /** Cleans up component */
    public void destroy() {
        mStateTracker.removeObserver(this);
        mTabBottomSheetManager.removePeekViewModel();
    }

    /** Called when the actor control button is clicked. */
    /* package */ void onActorControlClicked() {
        ActorTask activeTask = mStateTracker.getCurrentActiveTask();
        // TODO(crbug.com/503370476): Use ActorUiStateManager to track when tasks are finished,
        // instead of checking activeTask.
        if (activeTask == null) {
            // When a task finishes, PeekView transitions to the "WAITING" state. It will remain in
            // this state until the user dismisses it, so it is possible for a user to press the
            // actor control button when there is no active task.
            if (PeekViewUiState.WAITING.equals(mPeekViewUiState)) {
                // In the WAITING state, the actor control button is the "View" button.
                if (mStateTracker.getActuatedTabId() != Tab.INVALID_TAB_ID) {
                    mTabSelectionDelegate.switchToTab(mStateTracker.getActuatedTabId());
                }
                mTabBottomSheetManager.setSheetExpanded(true);
                // Reset the state to DEFAULT.
                mStateTracker.clearWaitingState();
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
