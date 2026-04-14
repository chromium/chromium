// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskId;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for the Actor Control component. */
@NullMarked
public class ActorControlCoordinator
        implements ActorUiTabController.Observer, ActorKeyedService.Observer {
    private static final String TAG = "ActorControlCoordin";

    private final ActorControlMediator mMediator;
    private final Context mContext;
    private final PropertyModel mModel;
    private final TabSupplierObserver mTabObserver;
    private final Callback<Profile> mProfileObserver;
    private final TabBottomSheetManager mTabBottomSheetManager;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;

    private @Nullable ActorKeyedService mActorKeyedService;
    private @Nullable ActorUiTabController mActorUiTabController;
    private @MonotonicNonNull PropertyModelChangeProcessor mViewBinder;
    private @MonotonicNonNull ActorControlView mView;

    /**
     * Constructs a new {@link ActorControlCoordinator}.
     *
     * @param context The {@link Context} used to inflate the layout.
     * @param tabSupplier The {@link ObservableSupplier<Tab>} for the activity.
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for the tab bottom sheet.
     * @param profileSupplier The {@link ObservableSupplier<Profile>} for the profile.
     */
    // TODO(crbug.com/491895203): Add render test for peek view.
    public ActorControlCoordinator(
            Context context,
            NullableObservableSupplier<Tab> tabSupplier,
            TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mTabBottomSheetManager = tabBottomSheetManager;
        mProfileSupplier = profileSupplier;

        mModel =
                new PropertyModel.Builder(ActorControlProperties.ALL_KEYS)
                        .with(ActorControlProperties.TASK_TITLE, "")
                        .with(ActorControlProperties.PEEK_VIEW_UI_STATE, PeekViewUiState.DEFAULT)
                        .with(
                                ActorControlProperties.ON_ACTOR_CONTROL_CLICKED,
                                this::onActorControlClicked)
                        .with(ActorControlProperties.ON_CLOSE_CLICKED, this::onCloseClicked)
                        .build();

        mMediator = new ActorControlMediator(mModel);
        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mActorUiTabController != null) {
                            mActorUiTabController.removeObserver(ActorControlCoordinator.this);
                        }
                        mActorUiTabController = tab != null ? ActorUiTabController.from(tab) : null;
                        if (mActorUiTabController != null) {
                            mActorUiTabController.addObserver(ActorControlCoordinator.this);
                            if (mView == null) {
                                attachPeekView();
                            }
                        }
                    }
                };

        mProfileObserver = this::onProfileAdded;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    private void onProfileAdded(Profile profile) {
        if (mActorKeyedService != null) {
            mActorKeyedService.removeObserver(ActorControlCoordinator.this);
            mActorKeyedService = null;
        }

        boolean isProfileValid =
                profile != null && profile.isNativeInitialized() && !profile.isOffTheRecord();
        if (!isProfileValid) {
            clearPeekViewContent();
            return;
        }

        mActorKeyedService = ActorKeyedServiceFactory.getForProfile(profile);
        if (mActorKeyedService == null) {
            clearPeekViewContent();
            return;
        }

        mActorKeyedService.addObserver(ActorControlCoordinator.this);
        ActorTask activeTask = mActorKeyedService.getCurrentActiveTask();
        if (activeTask != null) {
            onTaskStateChanged(activeTask.getId(), activeTask.getState());
        } else {
            clearPeekViewContent();
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

        ActorTask activeTask = mActorKeyedService.getCurrentActiveTask();
        if (activeTask == null) {
            clearPeekViewContent();
            return;
        }

        if (activeTask.getId() != taskId) {
            return;
        }

        String taskTitle = activeTask.getTitle() == null ? "" : activeTask.getTitle();
        switch (newState) {
            case ActorTaskState.ACTING:
            case ActorTaskState.REFLECTING:
                setPeekViewContent(taskTitle, PeekViewUiState.ACTING);
                break;
            case ActorTaskState.PAUSED_BY_USER:
                setPeekViewContent(taskTitle, PeekViewUiState.PAUSED);
                break;
            case ActorTaskState.PAUSED_BY_ACTOR:
            case ActorTaskState.WAITING_ON_USER:
                setPeekViewContent(taskTitle, PeekViewUiState.WAITING);
                break;
            case ActorTaskState.CREATED:
            case ActorTaskState.CANCELLED:
            case ActorTaskState.FINISHED:
            case ActorTaskState.FAILED:
                clearPeekViewContent();
                break;
            default:
                assert false : "Unhandled ActorTaskState " + newState;
                clearPeekViewContent();
                break;
        }
    }

    private void setPeekViewContent(String title, PeekViewUiState state) {
        mMediator.setContent(title, state);
    }

    private void clearPeekViewContent() {
        mMediator.setContent("", PeekViewUiState.DEFAULT);
    }

    /**
     * Called when the UI tab state changes.
     *
     * @param state The new UI tab state.
     */
    @Override
    public void onUiTabStateChanged(UiTabState state) {
        if (!mTabBottomSheetManager.isSheetInitialized()) {
            return;
        }
        if (state.actorOverlay.isActive) {
            if (mView == null) {
                attachPeekView();
            }
            if (!mTabBottomSheetManager.showPeekViewAndHideExpandedContent()) {
                Log.d(TAG, "onUiTabStateChanged: Failed to show peek view.");
            }
            return;
        } else {
            if (!mTabBottomSheetManager.hidePeekViewAndShowExpandedContent()) {
                Log.d(TAG, "onUiTabStateChanged: Failed to hide peek view.");
            }
        }
    }

    /**
     * Initializes peek view, if it is not already initialized, and attaches it to the bottom sheet.
     */
    public void attachPeekView() {
        assert mView == null;
        if (mTabBottomSheetManager == null || !mTabBottomSheetManager.isSheetInitialized()) {
            return;
        }
        mView =
                (ActorControlView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.actor_control_layout, null, false);
        mTabBottomSheetManager.attachPeekView(mView);
        mViewBinder =
                PropertyModelChangeProcessor.create(mModel, mView, ActorControlViewBinder::bind);
    }

    /** Cleans up component */
    public void destroy() {
        if (mViewBinder != null) {
            mViewBinder.destroy();
        }
        if (mActorUiTabController != null) {
            mActorUiTabController.removeObserver(this);
        }
        if (mActorKeyedService != null) {
            mActorKeyedService.removeObserver(this);
        }
        if (mProfileSupplier != null && mProfileObserver != null) {
            mProfileSupplier.removeObserver(mProfileObserver);
        }
        mTabObserver.destroy();
    }

    /** Called when the actor control button is clicked. */
    /* package */ void onActorControlClicked() {
        assert mActorKeyedService != null;

        ActorTask activeTask = mActorKeyedService.getCurrentActiveTask();
        // If there is no active task, the task has just finished.
        if (activeTask == null) {
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
                mTabBottomSheetManager.hidePeekViewAndShowExpandedContent();
                break;
            default:
                Log.w(
                        TAG,
                        "onActorControlClicked: Unhandled state "
                                + currentState
                                + " for task "
                                + activeTask.getId());
                break;
        }
    }

    /** Called when the close button is clicked. */
    /* package */ void onCloseClicked() {
        assert mTabBottomSheetManager.isSheetInitialized();
        mTabBottomSheetManager.tryToCloseBottomSheet();
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

    /**
     * @return The {@link View} for this component.
     */
    /* package */ @Nullable View getPeekViewForTesting() {
        return mView;
    }
}
