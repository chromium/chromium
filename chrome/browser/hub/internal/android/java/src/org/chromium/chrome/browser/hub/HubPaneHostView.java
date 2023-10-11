// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

/** Holds the current pane's {@link View}. */
public class HubPaneHostView extends FrameLayout {
    /** Default {@link FrameLayout} constructor. */
    public HubPaneHostView(Context context) {
        super(context);
    }
}
