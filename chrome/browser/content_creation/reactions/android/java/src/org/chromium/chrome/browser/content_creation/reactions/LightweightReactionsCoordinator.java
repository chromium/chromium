// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/**
 * Public interface for the lightweight reactions component that is responsible for the UI.
 */
public interface LightweightReactionsCoordinator extends BottomSheetObserver {
    /**
     * Displays the scene editor in a fullscreen dialog.
     */
    void showDialog();
}