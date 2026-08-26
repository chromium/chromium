// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.text.method.LinkMovementMethod;
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
            messageView.setMovementMethod(LinkMovementMethod.getInstance());
        } else if (TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER == propertyKey) {
            View stopButton = view.findViewById(R.id.tab_sharing_stop_button);
            Runnable listener = model.get(TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER);
            stopButton.setOnClickListener(listener == null ? null : v -> listener.run());
        } else if (TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE == propertyKey) {
            View shareInsteadButton = view.findViewById(R.id.tab_sharing_share_instead_button);
            shareInsteadButton.setVisibility(
                    model.get(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER == propertyKey) {
            View shareInsteadButton = view.findViewById(R.id.tab_sharing_share_instead_button);
            Runnable listener = model.get(TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER);
            shareInsteadButton.setOnClickListener(listener == null ? null : v -> listener.run());
        }
    }
}
