// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for the Actor Picture-in-Picture overlay. */
@NullMarked
public class ActorPictureInPictureOverlayCoordinator {
    private final ActorPictureInPictureOverlayView mView;
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final ActorPictureInPictureOverlayMediator mMediator;
    private final ViewGroup mParent;

    /**
     * Constructs a new {@link ActorPictureInPictureOverlayCoordinator}.
     *
     * @param context The {@link Context} used for inflation and resource retrieval.
     * @param parent The {@link ViewGroup} (usually the Activity root) where the overlay is added.
     */
    public ActorPictureInPictureOverlayCoordinator(Context context, ViewGroup parent) {
        mParent = parent;
        mView =
                (ActorPictureInPictureOverlayView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.actor_picture_in_picture_overlay, mParent, false);

        PropertyModel model =
                new PropertyModel.Builder(ActorPictureInPictureOverlayProperties.ALL_KEYS).build();

        mMediator = new ActorPictureInPictureOverlayMediator(context, model);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mView, ActorPictureInPictureOverlayViewBinder::bind);

        mParent.addView(mView);
    }

    /** Updates the displayed title of the task in the PiP overlay. */
    public void updateTitle(String title) {
        mMediator.updateTitle(title);
    }

    /** Updates the displayed status message based on the task's current execution state. */
    public void updateStatus(int state) {
        mMediator.updateStatus(state);
    }

    /** Updates visibility of overlay */
    public void setVisibility(boolean visible) {
        mMediator.setVisibility(visible);
    }

    /** Cleans up component */
    public void destroy() {
        mChangeProcessor.destroy();
        if (mView.getParent() != null) {
            mParent.removeView(mView);
        }
    }
}
