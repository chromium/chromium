// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * A {@link BackPressHandler} which observes fullscreen mode and exits fullscreen mode if back
 * press is performed.
 */
public class FullscreenBackPressHandler implements BackPressHandler {
    private final FullscreenManager mFullscreenManager;

    public FullscreenBackPressHandler(FullscreenManager fullscreenManager) {
        mFullscreenManager = fullscreenManager;
    }

    @Override
    public void handleBackPress() {
        mFullscreenManager.exitPersistentFullscreenMode();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mFullscreenManager.getPersistentFullscreenModeSupplier();
    }
}
