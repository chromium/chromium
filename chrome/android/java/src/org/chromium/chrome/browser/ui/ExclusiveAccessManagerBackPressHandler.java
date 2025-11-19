// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * A {@link BackPressHandler} which observes fullscreen mode and exits fullscreen mode if back press
 * is performed.
 */
@NullMarked
public class ExclusiveAccessManagerBackPressHandler implements BackPressHandler {
    private final ExclusiveAccessManager mExclusiveAccessManager;

    public ExclusiveAccessManagerBackPressHandler(ExclusiveAccessManager eam) {
        mExclusiveAccessManager = eam;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        int res =
                mExclusiveAccessManager.hasExclusiveAccess()
                        ? BackPressResult.SUCCESS
                        : BackPressResult.FAILURE;
        mExclusiveAccessManager.exitExclusiveAccess();
        return res;
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        // Back press and Esc press should be handled separately as Esc key handling has additional
        // logic in ExclusiveAccessManager native PreHandleKeyboardEvent function.
        return false;
    }

    @Override
    public @Nullable Boolean handleEscPress() {
        // Not handling Escape here to let it go to the native part for long key press handling
        return Boolean.FALSE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mExclusiveAccessManager.getExclusiveAccessStateSupplier();
    }
}
