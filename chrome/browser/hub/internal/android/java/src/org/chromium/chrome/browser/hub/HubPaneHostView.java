// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
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
        mActionButton = findViewById(R.id.host_action_button);
    }

    void setRootView(@Nullable View rootView) {
        mPaneFrame.removeAllViews();
        if (rootView != null) {
            mPaneFrame.addView(rootView);
        }
    }

    void setActionButtonData(@Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, mActionButton);
    }
}
