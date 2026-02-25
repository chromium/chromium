// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ChromeImageButton;

/** A {@link RelativeLayout} for actor control view. */
@NullMarked
public class ActorControlView extends RelativeLayout {
    private TextView mTitleView;
    private TextView mDescriptionView;
    private ChromeImageButton mStatusIcon;
    private ChromeImageButton mCloseIcon;

    public ActorControlView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.actor_control_title);
        mDescriptionView = findViewById(R.id.actor_control_description);
        mStatusIcon = findViewById(R.id.actor_control_status_button);
        mCloseIcon = findViewById(R.id.actor_control_close_button);
    }

    /**
     * Sets the main title text for the task.
     *
     * @param title The string to display.
     */
    void setTitle(String title) {
        mTitleView.setText(title);
    }

    /**
     * Sets the description text for the current step.
     *
     * @param description The string describing the active step of the task.
     */
    void setStepDescription(String description) {
        mDescriptionView.setText(description);
    }

    /**
     * Sets the click listener for the status (play/pause) button.
     *
     * @param listener The callback to be invoked when the status icon is clicked.
     */
    void setPlayPauseListener(OnClickListener listener) {
        mStatusIcon.setOnClickListener(listener);
    }

    /**
     * Sets the click listener for the close button.
     *
     * @param listener The callback to be invoked when the close icon is clicked.
     */
    void setCloseClickListener(OnClickListener listener) {
        mCloseIcon.setOnClickListener(listener);
    }

    /**
     * Sets the image resource for the status (play/pause) icon.
     *
     * @param resId The drawable resource ID to display.
     */
    void setStatusIconResource(int resId) {
        mStatusIcon.setImageResource(resId);
    }
}
