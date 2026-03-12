// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The root view for the Actor Picture-In-Picture Overlay. */
@NullMarked
public class ActorPictureInPictureOverlayView extends LinearLayout {
    private TextView mTitleView;
    private TextView mStatusView;

    /** Constructor for inflating from XML. */
    public ActorPictureInPictureOverlayView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.pip_title);
        mStatusView = findViewById(R.id.pip_status);
    }

    void setTitle(String title) {
        mTitleView.setText(title);
    }

    void setStatus(String status) {
        mStatusView.setText(status);
    }
}
