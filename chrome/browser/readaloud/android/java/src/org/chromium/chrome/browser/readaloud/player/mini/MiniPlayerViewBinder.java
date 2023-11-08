// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.View;

import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder as described in //docs/ui/android/mvc_overview.md. Updates views
 * based on model state.
 */
public class MiniPlayerViewBinder {
    static class ViewHolder {
        public MiniPlayerLayout view;
        public ReadAloudMiniPlayerSceneLayer sceneLayer;

        ViewHolder(MiniPlayerLayout view, ReadAloudMiniPlayerSceneLayer sceneLayer) {
            this.view = view;
            this.sceneLayer = sceneLayer;
        }
    }

    /**
     * Called by {@link PropertyModelChangeProcessor} on creation and each time the model is
     * updated.
     */
    public static void bindPlayerProperties(
            PropertyModel model, MiniPlayerLayout view, PropertyKey key) {
        if (key == PlayerProperties.TITLE) {
            view.setTitle(model.get(PlayerProperties.TITLE));

        } else if (key == PlayerProperties.PUBLISHER) {
            view.setPublisher(model.get(PlayerProperties.PUBLISHER));

        } else if (key == PlayerProperties.PLAYBACK_STATE) {
            view.onPlaybackStateChanged(model.get(PlayerProperties.PLAYBACK_STATE));

        } else if (key == PlayerProperties.PROGRESS) {
            view.setProgress(model.get(PlayerProperties.PROGRESS));

        } else if (key == PlayerProperties.INTERACTION_HANDLER) {
            view.setInteractionHandler(model.get(PlayerProperties.INTERACTION_HANDLER));
        }
    }

    public static void bindMiniPlayerProperties(
            PropertyModel model, ViewHolder viewHolder, PropertyKey key) {
        if (key == Properties.VISIBILITY) {
            // TODO: temporary measure to keep show and hide working during changes, remove
            // once mediator changes are in place.
            if (model.get(Properties.VISIBILITY) == VisibilityState.SHOWING) {
                viewHolder.view.setVisibility(View.VISIBLE);
                model.set(Properties.VISIBILITY, VisibilityState.VISIBLE);
            } else if (model.get(Properties.VISIBILITY) == VisibilityState.HIDING) {
                viewHolder.view.setVisibility(View.GONE);
                model.set(Properties.VISIBILITY, VisibilityState.GONE);
            }

        } else if (key == Properties.ANIMATE_VISIBILITY_CHANGES) {
            viewHolder.view.enableAnimations(model.get(Properties.ANIMATE_VISIBILITY_CHANGES));

        } else if (key == Properties.MEDIATOR) {
            viewHolder.view.setMediator(model.get(Properties.MEDIATOR));

        } else if (key == Properties.ANDROID_VIEW_VISIBILITY) {
            viewHolder.view.setVisibility(model.get(Properties.ANDROID_VIEW_VISIBILITY));

        } else if (key == Properties.COMPOSITED_VIEW_VISIBLE) {
            viewHolder.sceneLayer.setIsVisible(model.get(Properties.COMPOSITED_VIEW_VISIBLE));
        } else if (key == Properties.CONTENTS_OPAQUE) {
            if (model.get(Properties.CONTENTS_OPAQUE)) {
                viewHolder.view.changeOpacity(/* startValue= */ 0f, /* endValue= */ 1f);
            } else {
                viewHolder.view.changeOpacity(/* startValue= */ 1f, /* endValue= */ 0f);
            }
        }
    }
}
