// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.View;

import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud mini player. */
public class MiniPlayerMediator {
    private final PropertyModel mModel;
    private final BrowserControlsSizer mBrowserControlsSizer;

    MiniPlayerMediator(BrowserControlsSizer browserControlsSizer) {
        mModel =
                new PropertyModel.Builder(Properties.ALL_KEYS)
                        .with(Properties.VISIBILITY, VisibilityState.GONE)
                        .with(Properties.ANDROID_VIEW_VISIBILITY, View.GONE)
                        .with(Properties.COMPOSITED_VIEW_VISIBLE, false)
                        .with(Properties.MEDIATOR, this)
                        .build();
        mBrowserControlsSizer = browserControlsSizer;
    }

    PropertyModel getModel() {
        return mModel;
    }

    @VisibilityState
    int getVisibility() {
        return mModel.get(Properties.VISIBILITY);
    }

    void show(boolean animate) {
        mModel.set(Properties.ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(Properties.VISIBILITY, VisibilityState.SHOWING);
    }

    void dismiss(boolean animate) {
        mModel.set(Properties.ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(Properties.VISIBILITY, VisibilityState.HIDING);
    }

    /**
     * Called when the view visibility changes due to animation.
     *
     * @param newState New visibility.
     */
    public void onVisibilityChanged(@VisibilityState int newState) {
        mModel.set(Properties.VISIBILITY, newState);
    }
}
