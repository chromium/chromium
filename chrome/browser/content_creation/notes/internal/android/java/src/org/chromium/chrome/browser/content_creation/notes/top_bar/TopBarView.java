// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.top_bar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.content_creation.internal.R;

/** The view for the top bar. */
public class TopBarView extends FrameLayout {
    public TopBarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Set listener for close button. */
    void setOnCloseListener(Runnable listener) {
        View button = findViewById(R.id.close);
        button.setOnClickListener(v -> listener.run());
    }

    /** Set listener for next button. */
    void setOnNextListener(Runnable listener) {
        View button = findViewById(R.id.next);
        button.setOnClickListener(v -> listener.run());
    }
}
