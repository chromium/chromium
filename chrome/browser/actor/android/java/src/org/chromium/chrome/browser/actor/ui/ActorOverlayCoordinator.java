// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;
import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
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
     */
    public ActorOverlayCoordinator(ViewStub viewStub) {
        mView = (ActorOverlayView) viewStub.inflate();

        mModel = new PropertyModel.Builder(ActorOverlayProperties.ALL_KEYS).build();
        mChangeProcessor = PropertyModelChangeProcessor.create(mModel, mView, ActorOverlayViewBinder::bind);

        mMediator = new ActorOverlayMediator(mModel);
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
        mChangeProcessor.destroy();
    }
}
