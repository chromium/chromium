// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for the Actor Control component. */
@NullMarked
public class ActorControlCoordinator {
    private final ActorControlMediator mMediator;
    private final PropertyModelChangeProcessor mViewBinder;
    private final ActorControlView mView;
    private final PropertyModel mModel;

    /**
     * Constructs a new {@link ActorControlCoordinator}.
     *
     * @param context The {@link Context} used to inflate the layout.
     * @param parent The {@link FrameLayout} where this component's view will be attached.
     * @param playPauseListener The {@link View.OnClickListener} for the status button.
     * @param closeListener The {@link View.OnClickListener} for the close button.
     */
    public ActorControlCoordinator(
            Context context,
            FrameLayout parent,
            View.OnClickListener playPauseListener,
            View.OnClickListener closeListener) {
        // TODO(crbug.com/486286959): Replace FrameLayout with specific parent view.
        mView =
                (ActorControlView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.actor_control_layout, parent, false);
        parent.addView(mView);

        mModel =
                new PropertyModel.Builder(ActorControlProperties.ALL_KEYS)
                        .with(ActorControlProperties.TASK_TITLE, "")
                        .with(ActorControlProperties.TASK_STEP_DESCRIPTION, "")
                        .with(
                                ActorControlProperties.STATUS_ICON_RESOURCE,
                                R.drawable.ic_pause_white_24dp)
                        .with(ActorControlProperties.ON_PLAY_PAUSE_CLICKED, playPauseListener)
                        .with(ActorControlProperties.ON_CLOSE_CLICKED, closeListener)
                        .build();

        mMediator = new ActorControlMediator(mModel);

        mViewBinder =
                PropertyModelChangeProcessor.create(mModel, mView, ActorControlViewBinder::bind);
    }

    /** Cleans up component */
    public void destroy() {
        mViewBinder.destroy();
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
}
