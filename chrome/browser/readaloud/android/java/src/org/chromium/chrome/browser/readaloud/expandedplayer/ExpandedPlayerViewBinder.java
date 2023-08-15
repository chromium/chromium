// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import org.chromium.chrome.modules.readaloud.ExpandedPlayer.State;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder as described in //docs/ui/android/mvc_overview.md. Updates views
 * based on model state.
 */
public class ExpandedPlayerViewBinder {
    /** Called by {@link PropertyModelChangeProcessor} each time the model is updated. */
    public static void bind(
            PropertyModel model, ExpandedPlayerSheetContent content, PropertyKey key) {
        if (key == ExpandedPlayerProperties.STATE_KEY) {
            @State
            int state = model.get(ExpandedPlayerProperties.STATE_KEY);
            if (state == State.SHOWING) {
                content.show();
            } else if (state == State.HIDING) {
                content.hide();
            }
        } else if (key == ExpandedPlayerProperties.SPEED_KEY) {
            content.setSpeed(model.get(ExpandedPlayerProperties.SPEED_KEY));
        }
    }
}
