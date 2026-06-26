// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static java.util.Collections.emptySet;

import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskId;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Collection;

/** Coordinates the Actor Overlay component. */
@NullMarked
public class ActorOverlayCoordinator {
    private final ActorOverlayMediator mMediator;
    private final ActorOverlayView mView;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final SnackbarManager mSnackbarManager;
    private final BackPressHandlerRegistry mBackPressHandlerRegistry;
    private final SnackbarManager.SnackbarController mSnackbarController;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileObserver;
    private final TabModelSelector mTabModelSelector;
    private final @Nullable SideUiStateProvider mSideUiStateProvider;
    private @Nullable SideUiObserver mSideUiObserver;
    private @Nullable ActorKeyedService mActorKeyedService;
    private ActorKeyedService.@Nullable Observer mActorObserver;

    /**
     * Constructs the Coordinator.
     *
     * @param viewStub The ViewStub to inflate the overlay into.
     * @param tabModelSelector The TabModelSelector to observe.
     * @param browserControlsVisibilityManager The BrowserControlsVisibilityManager to observe.
     * @param tabObscuringHandler The TabObscuringHandler to obscure the web content.
     * @param snackbarManager The SnackbarManager to show the snackbar.
     * @param backPressHandlerRegistry The BackPressHandlerRegistry to handle back press.
     * @param layoutManagerSupplier The LayoutManager supplier to observe layout changes.
     * @param profileSupplier The Profile supplier to observe profile changes.
     * @param sideUiStateProvider The {@link SideUiStateProvider} providing state on the side UI.
     */
    public ActorOverlayCoordinator(
            ViewStub viewStub,
            TabModelSelector tabModelSelector,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler,
            SnackbarManager snackbarManager,
            BackPressHandlerRegistry backPressHandlerRegistry,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier,
            MonotonicObservableSupplier<Profile> profileSupplier,
            @Nullable SideUiStateProvider sideUiStateProvider) {
        mView = (ActorOverlayView) viewStub.inflate();
        mSnackbarManager = snackbarManager;
        mBackPressHandlerRegistry = backPressHandlerRegistry;
        mProfileSupplier = profileSupplier;
        mTabModelSelector = tabModelSelector;

        mModel =
                new PropertyModel.Builder(ActorOverlayProperties.ALL_KEYS)
                        .with(ActorOverlayProperties.VISIBLE, false)
                        .with(ActorOverlayProperties.LEFT_MARGIN, 0)
                        .with(ActorOverlayProperties.TOP_MARGIN, 0)
                        .with(ActorOverlayProperties.RIGHT_MARGIN, 0)
                        .with(ActorOverlayProperties.BOTTOM_MARGIN, 0)
                        .with(ActorOverlayProperties.ON_CLICK_LISTENER, v -> handleOnClick())
                        .with(
                                ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER,
                                v -> handleTakeOverTask())
                        .with(ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE, false)
                        .build();

        mSideUiStateProvider = sideUiStateProvider;
        if (mSideUiStateProvider != null) {
            mSideUiObserver = new MarginAdjusterForSideUi(mView, mModel);
            mSideUiStateProvider.addObserver(mSideUiObserver);
        }

        mChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mView, ActorOverlayViewBinder::bind);

        // Empty impl, used to dismiss a named snackbar.
        mSnackbarController = new SnackbarManager.SnackbarController() {};

        mMediator =
                new ActorOverlayMediator(
                        mModel,
                        tabModelSelector,
                        browserControlsVisibilityManager,
                        tabObscuringHandler,
                        layoutManagerSupplier,
                        this::showInteractionLimitedSnackbar,
                        this::dismissInteractionLimitedSnackbar);
        mBackPressHandlerRegistry.addHandler(mMediator, BackPressHandler.Type.ACTOR_OVERLAY);

        mProfileObserver = this::onProfileAdded;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    private static class MarginAdjusterForSideUi implements SideUiObserver {
        private final View mView;
        private final PropertyModel mModel;

        MarginAdjusterForSideUi(View view, PropertyModel model) {
            mView = view;
            mModel = model;
        }

        @Override
        public @Nullable Transition onPreSideUiSpecsChange(
                SideUiCoordinator.SideUiSpecs sideUiSpecs) {
            TransitionSet transitionSet = new TransitionSet();
            Collection<View> descendants = new ArrayList<>();
            ViewUtils.getAllDescendants(mView, descendants, emptySet());

            Transition transition = new ChangeBounds();
            transition.addTarget(mView);
            for (View view : descendants) {
                transition.addTarget(view);
            }
            transitionSet.addTransition(transition);
            return transitionSet;
        }

        @Override
        public void onSideUiSpecsChanged(SideUiCoordinator.SideUiSpecs sideUiSpecs) {
            mModel.set(ActorOverlayProperties.LEFT_MARGIN, sideUiSpecs.getWidth(AnchorSide.LEFT));
            mModel.set(ActorOverlayProperties.RIGHT_MARGIN, sideUiSpecs.getWidth(AnchorSide.RIGHT));
        }
    }

    private void onProfileAdded(Profile profile) {
        if (mActorKeyedService != null && mActorObserver != null) {
            mActorKeyedService.removeObserver(mActorObserver);
            mActorKeyedService = null;
            mActorObserver = null;
        }

        if (profile == null || profile.isOffTheRecord()) return;

        mActorKeyedService = ActorKeyedServiceFactory.getForProfile(profile);
        if (mActorKeyedService == null) return;

        mActorObserver =
                new ActorKeyedService.Observer() {
                    @Override
                    public void onTaskStateChanged(
                            @ActorTaskId int taskId, @ActorTaskState int newState) {
                        mMediator.onTaskStateChanged();
                    }
                };
        mActorKeyedService.addObserver(mActorObserver);
    }

    private void handleOnClick() {
        showInteractionLimitedSnackbar();
    }

    private void handleTakeOverTask() {
        assert mActorKeyedService != null;
        int tabId = mTabModelSelector.getCurrentTabId();
        if (tabId == Tab.INVALID_TAB_ID) return;

        Integer taskId = mActorKeyedService.getActiveTaskIdOnTab(tabId);
        if (taskId != null) {
            ActorTask task = mActorKeyedService.getTask(taskId);
            if (task != null) {
                task.takeOverTask();
            }
        }
    }

    private void showInteractionLimitedSnackbar() {
        if (mSnackbarManager.isShowing()) return;

        Snackbar snackbar =
                Snackbar.make(
                        mView.getContext().getString(R.string.actor_overlay_snackbar_message),
                        mSnackbarController,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_ACTOR);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void dismissInteractionLimitedSnackbar() {
        mSnackbarManager.dismissSnackbars(mSnackbarController);
    }

    /** Returns the root view of the overlay. */
    public View getView() {
        return mView;
    }

    /** Returns the mediator for the overlay. */
    public ActorOverlayMediator getMediator() {
        return mMediator;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    /** Sets the visibility of the overlay for testing purposes. */
    public void showOverlayForTesting(boolean visible) {
        mMediator.setOverlayVisible(visible);
    }

    /** Cleans up the coordinator. */
    public void destroy() {
        if (mSideUiStateProvider != null && mSideUiObserver != null) {
            mSideUiStateProvider.removeObserver(mSideUiObserver);
        }
        if (mActorKeyedService != null && mActorObserver != null) {
            mActorKeyedService.removeObserver(mActorObserver);
        }
        if (mProfileSupplier != null && mProfileObserver != null) {
            mProfileSupplier.removeObserver(mProfileObserver);
        }
        mBackPressHandlerRegistry.removeHandler(mMediator);
        mMediator.destroy();
        mChangeProcessor.destroy();
        dismissInteractionLimitedSnackbar();
    }
}
