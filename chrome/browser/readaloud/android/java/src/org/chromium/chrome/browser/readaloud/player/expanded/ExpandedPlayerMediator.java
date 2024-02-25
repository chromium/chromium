// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud expanded player. */
public class ExpandedPlayerMediator extends EmptyBottomSheetObserver {
    private final PropertyModel mModel;

    public ExpandedPlayerMediator(PropertyModel model) {
        mModel = model;
        setVisibility(VisibilityState.HIDING);
    }

    public void show() {
        @VisibilityState int state = getVisibility();
        if (state == VisibilityState.SHOWING || state == VisibilityState.VISIBLE) {
            return;
        }
        setVisibility(VisibilityState.SHOWING);
        setShowMiniPlayerOnDismiss(true);
    }

    public void dismiss() {
        @VisibilityState int state = getVisibility();
        if (state == VisibilityState.GONE || state == VisibilityState.HIDING) {
            return;
        }
        setVisibility(VisibilityState.HIDING);
    }

    public @VisibilityState int getVisibility() {
        return mModel.get(PlayerProperties.EXPANDED_PLAYER_VISIBILITY);
    }

    void setVisibility(@VisibilityState int state) {
        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, state);
    }

    void setShowMiniPlayerOnDismiss(boolean value) {
        mModel.set(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS, value);
    }

    boolean getShowMiniPlayerOnDismiss() {
        return mModel.get(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS);
    }

    void setHiddenAndPlaying(boolean value) {
        mModel.set(PlayerProperties.HIDDEN_AND_PLAYING, value);
    }
}
