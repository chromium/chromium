// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * A base interface representing a UI that will be displayed as a Pane in the Hub.
 */
public interface Pane extends BackPressHandler {
    /** Returns the {@link View} containing the contents of the Pane. */
    public View getRootView();
}
