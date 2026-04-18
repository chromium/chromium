// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;
import android.view.ViewStub;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
     */
    public ActorOverlayCoordinator(
            ViewStub viewStub,
            TabModelSelector tabModelSelector,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler,
            SnackbarManager snackbarManager,
            BackPressHandlerRegistry backPressHandlerRegistry,
            MonotonicObservableSupplier<LayoutManager> layoutManagerSupplier) {
        mView = (ActorOverlayView) viewStub.inflate();
        mSnackbarManager = snackbarManager;
        mBackPressHandlerRegistry = backPressHandlerRegistry;

        mModel =
                new PropertyModel.Builder(ActorOverlayProperties.ALL_KEYS)
                        .with(ActorOverlayProperties.VISIBLE, false)
                        .with(ActorOverlayProperties.CAN_SHOW, true)
                        .with(ActorOverlayProperties.TOP_MARGIN, 0)
                        .with(ActorOverlayProperties.BOTTOM_MARGIN, 0)
                        .with(ActorOverlayProperties.ON_CLICK_LISTENER, v -> handleOnClick())
                        .build();
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
    }

    private void handleOnClick() {
        showInteractionLimitedSnackbar();
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
        mBackPressHandlerRegistry.removeHandler(mMediator);
        mMediator.destroy();
        mChangeProcessor.destroy();
        dismissInteractionLimitedSnackbar();
    }
}
