// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.ui.modelutil.PropertyModel;

/** The Mediator for the Actor Picture-in-Picture overlay. */
@NullMarked
class ActorPictureInPictureOverlayMediator {
    private final PropertyModel mModel;
    private final Context mContext;

    ActorPictureInPictureOverlayMediator(Context context, PropertyModel model) {
        mContext = context;
        mModel = model;
    }

    void updateTitle(String taskTitle) {
        mModel.set(ActorPictureInPictureOverlayProperties.TITLE, taskTitle);
    }

    void updateStatus(@ActorTaskState int state) {
        String status;
        switch (state) {
            case ActorTaskState.PAUSED_BY_ACTOR:
            case ActorTaskState.PAUSED_BY_USER:
                status = mContext.getString(R.string.actor_pip_paused_status);
                break;

            case ActorTaskState.WAITING_ON_USER:
                status = mContext.getString(R.string.actor_pip_needs_attention_status);
                break;

            case ActorTaskState.CANCELLED:
            case ActorTaskState.FAILED:
                status = mContext.getString(R.string.actor_notification_title_task_interrupted);
                break;
            case ActorTaskState.FINISHED:
                status = mContext.getString(R.string.actor_pip_complete_status);
                break;

            case ActorTaskState.ACTING:
            case ActorTaskState.REFLECTING:
            case ActorTaskState.CREATED:
            default:
                status = mContext.getString(R.string.actor_pip_working_status);
                break;
        }
        mModel.set(ActorPictureInPictureOverlayProperties.STATUS_TEXT, status);
    }

    void setVisibility(boolean visible) {
        mModel.set(ActorPictureInPictureOverlayProperties.VISIBLE, visible);
    }
}
