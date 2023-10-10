// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud mini player. */
public class MiniPlayerMediator {
    private final PropertyModel mModel;

    MiniPlayerMediator(PropertyModel model) {
        mModel = model;
        mModel.set(PlayerProperties.MINI_PLAYER_MEDIATOR, this);
    }

    @VisibilityState
    int getVisibility() {
        return mModel.get(PlayerProperties.MINI_PLAYER_VISIBILITY);
    }

    void show(boolean animate) {
        mModel.set(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(PlayerProperties.MINI_PLAYER_VISIBILITY, VisibilityState.SHOWING);
    }

    void dismiss(boolean animate) {
        mModel.set(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(PlayerProperties.MINI_PLAYER_VISIBILITY, VisibilityState.HIDING);
    }

    /**
     * Called when the view visibility changes due to animation.
     *
     * @param newState New visibility.
     */
    public void onVisibilityChanged(@VisibilityState int newState) {
        mModel.set(PlayerProperties.MINI_PLAYER_VISIBILITY, newState);
    }
}
