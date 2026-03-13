// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Container for the bottom bar.
 *
 * <p>Note that the {@link BackPressHandler} implementation is left as default on purpose.
 */
@NullMarked
public class BottomBarContainerCoordinator implements BottomControlsContentDelegate {
    @SuppressWarnings("unused")
    private final FrameLayout mBottomBarContainer;

    private BottomControlsVisibilityController mVisibilityController;

    @SuppressWarnings("unused")
    private Callback<Object> mOnModelTokenChange;

    /**
     * @param bottomBarContainer The {@link FrameLayout} for the bottom bar.
     */
    public BottomBarContainerCoordinator(FrameLayout bottomBarContainer) {
        mBottomBarContainer = bottomBarContainer;
    }

    @Initializer
    @Override
    public void initializeWithNative(
            BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        mVisibilityController = visibilityController;
        mOnModelTokenChange = onModelTokenChange;

        mVisibilityController.setBottomControlsVisible(true);
    }

    @Override
    public void destroy() {}
}
