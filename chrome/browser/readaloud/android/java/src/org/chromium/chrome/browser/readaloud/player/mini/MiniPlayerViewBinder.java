// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.View;

import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder as described in //docs/ui/android/mvc_overview.md. Updates views
 * based on model state.
 */
public class MiniPlayerViewBinder {
    /**
     * Called by {@link PropertyModelChangeProcessor} on creation and each time the
     * model is updated.
     */
    public static void bind(PropertyModel model, MiniPlayerLayout view, PropertyKey key) {
        if (key == PlayerProperties.MINI_PLAYER_VISIBILITY) {
            view.setVisibility(
                    model.get(PlayerProperties.MINI_PLAYER_VISIBILITY) == VisibilityState.VISIBLE
                            ? View.VISIBLE
                            : View.GONE);

        } else if (key == PlayerProperties.PLAYBACK_STATE) {
            view.onPlaybackStateChanged(model.get(PlayerProperties.PLAYBACK_STATE));

        } else if (key == PlayerProperties.INTERACTION_HANDLER) {
            view.setInteractionHandler(model.get(PlayerProperties.INTERACTION_HANDLER));
        }
    }
}
