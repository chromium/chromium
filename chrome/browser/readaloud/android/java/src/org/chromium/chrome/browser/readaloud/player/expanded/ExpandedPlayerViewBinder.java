// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
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
            content.onPlaybackStateChanged(model.get(PlayerProperties.PLAYBACK_STATE));
        } else if (key == PlayerProperties.PROGRESS) {
            content.setProgress(model.get(PlayerProperties.PROGRESS));
        } else if (key == PlayerProperties.SPEED) {
            content.setSpeed(model.get(PlayerProperties.SPEED));
        } else if (key == PlayerProperties.ELAPSED_NANOS) {
            content.setElapsed(model.get(PlayerProperties.ELAPSED_NANOS));
        } else if (key == PlayerProperties.DURATION_NANOS) {
            content.setDuration(model.get(PlayerProperties.DURATION_NANOS));
        } else if (key == PlayerProperties.INTERACTION_HANDLER) {
            content.setInteractionHandler(model.get(PlayerProperties.INTERACTION_HANDLER));
        } else if (key == PlayerProperties.HIGHLIGHTING_ENABLED) {
            content.setHighlightingEnabled(model.get(PlayerProperties.HIGHLIGHTING_ENABLED));
        } else if (key == PlayerProperties.HIGHLIGHTING_SUPPORTED) {
            content.setHighlightingSupported(model.get(PlayerProperties.HIGHLIGHTING_SUPPORTED));
        } else if (key == PlayerProperties.VOICES_LIST) {
            if (content.getVoiceMenu() != null) {
                content.getVoiceMenu().setVoices(model.get(PlayerProperties.VOICES_LIST));
            }
        } else if (key == PlayerProperties.SELECTED_VOICE_ID) {
            if (content.getVoiceMenu() != null) {
                content.getVoiceMenu()
                        .setVoiceSelection(model.get(PlayerProperties.SELECTED_VOICE_ID));
            }
        } else if (key == PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE) {
            if (content.getVoiceMenu() != null) {
                content.getVoiceMenu()
                        .updatePreviewButtons(
                                model.get(PlayerProperties.PREVIEWING_VOICE_ID),
                                model.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
            }
        }
    }
}
