// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder as described in //docs/ui/android/mvc_overview.md. Updates views based on model
 * state.
 */
public class ExpandedPlayerViewBinder {
    /**
     * Called by {@link PropertyModelChangeProcessor} on creation and each time the model is
     * updated.
     */
    public static void bind(
            PropertyModel model, ExpandedPlayerSheetContent content, PropertyKey key) {
        if (key == PlayerProperties.EXPANDED_PLAYER_VISIBILITY) {
            @VisibilityState int state = model.get(PlayerProperties.EXPANDED_PLAYER_VISIBILITY);
            if (state == VisibilityState.SHOWING) {
                content.show();
            } else if (state == VisibilityState.HIDING) {
                content.hide();
            }
        } else if (key == PlayerProperties.TITLE) {
            content.setTitle(model.get(PlayerProperties.TITLE));
        } else if (key == PlayerProperties.PUBLISHER) {
            content.setPublisher(model.get(PlayerProperties.PUBLISHER));
        } else if (key == PlayerProperties.PLAYBACK_STATE) {
            content.setPlaying(
                    model.get(PlayerProperties.PLAYBACK_STATE) == PlaybackListener.State.PLAYING);
        } else if (key == PlayerProperties.SPEED) {
            content.setSpeed(model.get(PlayerProperties.SPEED));
        } else if (key == PlayerProperties.INTERACTION_HANDLER) {
            content.setInteractionHandler(model.get(PlayerProperties.INTERACTION_HANDLER));
        }
    }
}
