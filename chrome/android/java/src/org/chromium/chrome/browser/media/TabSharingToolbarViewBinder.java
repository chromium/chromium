// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the TabSharingToolbar. */
@NullMarked
class TabSharingToolbarViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (TabSharingToolbarProperties.STATUS_TEXT == propertyKey) {
            TextView messageView = view.findViewById(R.id.tab_sharing_message);
            messageView.setText(model.get(TabSharingToolbarProperties.STATUS_TEXT));
            messageView.setMovementMethod(android.text.method.LinkMovementMethod.getInstance());
        } else if (TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER == propertyKey) {
            View stopButton = view.findViewById(R.id.tab_sharing_stop_button);
            stopButton.setOnClickListener(
                    (v) -> {
                        Runnable listener =
                                model.get(TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER);
                        if (listener != null) listener.run();
                    });
        }
    }
}
