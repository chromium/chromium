// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud mini player. */
class MiniPlayerMediator {
    private final PropertyModel mModel;

    MiniPlayerMediator(PropertyModel model) {
        mModel = model;
    }

    @VisibilityState
    int getVisibility() {
        return mModel.get(PlayerProperties.MINI_PLAYER_VISIBILITY);
    }

    void show(boolean animate) {
        // TODO implement animation
        mModel.set(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(PlayerProperties.MINI_PLAYER_VISIBILITY, VisibilityState.VISIBLE);
    }

    void dismiss(boolean animate) {
        // TODO implement animation
        mModel.set(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(PlayerProperties.MINI_PLAYER_VISIBILITY, VisibilityState.GONE);
    }
}
