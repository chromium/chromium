// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.readaloud.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder as described in //docs/ui/android/mvc_overview.md. Updates views
 * based on model state.
 */
public class MiniPlayerViewBinder {
    /** Called by {@link PropertyModelChangeProcessor} each time the model is updated. */
    public static void bind(PropertyModel model, LinearLayout view, PropertyKey key) {
        if (key == MiniPlayerProperties.VIEW_VISIBILITY_KEY) {
            view.setVisibility(model.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
        } else if (key == MiniPlayerProperties.ON_CLOSE_CLICK_KEY) {
            view.findViewById(R.id.readaloud_mini_player_close_button)
                    .setOnClickListener(model.get(MiniPlayerProperties.ON_CLOSE_CLICK_KEY));
        } else if (key == MiniPlayerProperties.ON_EXPAND_CLICK_KEY) {
            view.findViewById(R.id.readaloud_mini_player_title_and_publisher)
                    .setOnClickListener(model.get(MiniPlayerProperties.ON_EXPAND_CLICK_KEY));
        } else if (key == MiniPlayerProperties.TITLE_KEY) {
            ((TextView) view.findViewById(R.id.readaloud_mini_player_title))
                    .setText(model.get(MiniPlayerProperties.TITLE_KEY));
        } else if (key == MiniPlayerProperties.PUBLISHER_KEY) {
            ((TextView) view.findViewById(R.id.readaloud_mini_player_publisher))
                    .setText(model.get(MiniPlayerProperties.PUBLISHER_KEY));
        }
    }
}
