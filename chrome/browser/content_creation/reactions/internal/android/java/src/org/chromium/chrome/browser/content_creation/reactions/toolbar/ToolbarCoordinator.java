// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.toolbar;

import android.view.View;
import android.widget.RelativeLayout;

import org.chromium.chrome.browser.content_creation.reactions.internal.R;

/**
 * Coordinator for the Lightweight Reactions toolbar.
 */
public class ToolbarCoordinator {
    public ToolbarCoordinator(View parentView, ToolbarControlsDelegate delegate) {
        RelativeLayout toolbarLayout = parentView.findViewById(R.id.lightweight_reactions_toolbar);

        View closeButton = toolbarLayout.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> delegate.cancelButtonTapped());
        View doneButton = toolbarLayout.findViewById(R.id.done_button);
        doneButton.setOnClickListener(v -> delegate.doneButtonTapped());
    }
}
