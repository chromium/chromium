// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ChromeImageButton;

/** A {@link RelativeLayout} for actor control view. */
@NullMarked
public class ActorControlView extends RelativeLayout {
    private TextView mTitleView;
    private TextView mDescriptionView;
    private MaterialButton mActorControlButton;
    private ChromeImageButton mCloseIcon;

    public ActorControlView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.actor_control_title);
        mDescriptionView = findViewById(R.id.actor_control_description);
        mActorControlButton = findViewById(R.id.actor_control_button);
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
     * @param visibility The visibility of the description view.
     */
    void setDescriptionView(String description, @PeekViewUiState.Visibility int visibility) {
        mDescriptionView.setText(description);
        mDescriptionView.setVisibility(visibility);
    }

    /**
     * Sets the click listener for the actor control button.
     *
     * @param listener The callback to be invoked when the actor control button is clicked.
     */
    void setActorControlClickListener(OnClickListener listener) {
        mActorControlButton.setOnClickListener(listener);
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
     * Sets the click listener for the peek view.
     *
     * @param listener The callback to be invoked when the peek view is clicked.
     */
    void setPeekViewClickListener(OnClickListener listener) {
        setOnClickListener(listener);
    }

    /**
     * Configures the actor control button for the given state.
     *
     * @param state The state of the peek view UI.
     */
    void configurePeekViewForState(PeekViewUiState state) {
        Context context = getContext();
        setDescriptionView(state.getDescription(context), state.getDescriptionVisibility());
        mTitleView.setTextAppearance(context, state.getTitleTextAppearanceResId());

        mActorControlButton.setVisibility(state.getButtonVisibility());
        mActorControlButton.setText(state.getButtonText(context));
        mActorControlButton.setIconResource(state.buttonIconResId);
        mActorControlButton.setBackgroundTintList(state.getButtonBackgroundTint(context));
        mActorControlButton.setIconTint(state.getIconTint(context));
        int horizontalPadding = state.getButtonHorizontalPadding(context);
        mActorControlButton.setPaddingRelative(horizontalPadding, 0, horizontalPadding, 0);
    }

    String getStepDescriptionForTesting() {
        return mDescriptionView.getText().toString();
    }
}
