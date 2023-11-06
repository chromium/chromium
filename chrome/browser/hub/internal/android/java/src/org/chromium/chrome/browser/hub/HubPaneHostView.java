// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

/** Holds the current pane's {@link View}. */
public class HubPaneHostView extends FrameLayout {
    private FrameLayout mPaneFrame;
    private Button mActionButton;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mPaneFrame = findViewById(R.id.pane_frame);
        mActionButton = findViewById(R.id.action_button);
    }

    void setRootView(@Nullable View rootView) {
        mPaneFrame.removeAllViews();
        if (rootView != null) {
            mPaneFrame.addView(rootView);
        }
    }

    void setActionButtonData(@Nullable FullButtonData buttonData) {
        if (buttonData == null) {
            mActionButton.setVisibility(View.GONE);
            mActionButton.setText(null);
            mActionButton.setOnClickListener(null);
            setStartDrawable(mActionButton, null);
        } else {
            Context context = getContext();
            mActionButton.setVisibility(View.VISIBLE);
            mActionButton.setText(buttonData.resolveText(context));
            mActionButton.setOnClickListener((v) -> buttonData.getOnPressRunnable().run());
            setStartDrawable(mActionButton, buttonData.resolveIcon(context));
        }
    }

    private static void setStartDrawable(Button button, Drawable drawable) {
        button.setCompoundDrawablesRelativeWithIntrinsicBounds(drawable, null, null, null);
    }
}
