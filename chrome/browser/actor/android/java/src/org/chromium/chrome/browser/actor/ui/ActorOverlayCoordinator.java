// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;
import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinates the Actor Overlay component. */
@NullMarked
public class ActorOverlayCoordinator {
    private final ActorOverlayMediator mMediator;
    private final ActorOverlayView mView;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mChangeProcessor;

    /**
     * Constructs the Coordinator.
     *
     * @param viewStub The ViewStub to inflate the overlay into.
     * @param tabModelSelector The TabModelSelector to observe.
     * @param browserControlsVisibilityManager The BrowserControlsVisibilityManager to observe.
     * @param tabObscuringHandler The TabObscuringHandler to obscure the web content.
     */
    public ActorOverlayCoordinator(
            ViewStub viewStub,
            TabModelSelector tabModelSelector,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabObscuringHandler tabObscuringHandler) {
        mView = (ActorOverlayView) viewStub.inflate();

        mModel =
                new PropertyModel.Builder(ActorOverlayProperties.ALL_KEYS)
                        .with(ActorOverlayProperties.VISIBLE, false)
                        .with(ActorOverlayProperties.CAN_SHOW, true)
                        .with(ActorOverlayProperties.TOP_MARGIN, 0)
                        .with(ActorOverlayProperties.BOTTOM_MARGIN, 0)
                        .build();
        mChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mView, ActorOverlayViewBinder::bind);

        mMediator =
                new ActorOverlayMediator(
                        mModel,
                        tabModelSelector,
                        browserControlsVisibilityManager,
                        tabObscuringHandler);
    }

    /** Returns the root view of the overlay. */
    public View getView() {
        return mView;
    }

    /** Returns the mediator for the overlay. */
    public ActorOverlayMediator getMediator() {
        return mMediator;
    }

    /** Cleans up the coordinator. */
    public void destroy() {
        mMediator.destroy();
        mChangeProcessor.destroy();
    }
}
