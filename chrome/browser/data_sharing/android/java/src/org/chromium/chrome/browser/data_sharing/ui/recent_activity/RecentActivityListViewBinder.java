// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.AVATAR_PROVIDER;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.FAVICON_PROVIDER;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.TITLE_TEXT;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the single recent activity row UI. */
class RecentActivityListViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (TITLE_TEXT == propertyKey) {
            ((TextView) view.findViewById(R.id.title)).setText(model.get(TITLE_TEXT));
        } else if (DESCRIPTION_TEXT == propertyKey) {
            ((TextView) view.findViewById(R.id.description)).setText(model.get(DESCRIPTION_TEXT));
        } else if (ON_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ON_CLICK_LISTENER));
        } else if (FAVICON_PROVIDER == propertyKey) {
            ImageView faviconView = view.findViewById(R.id.favicon);
            faviconView.setImageDrawable(null);
            model.get(FAVICON_PROVIDER).onResult(faviconView);
        } else if (AVATAR_PROVIDER == propertyKey) {
            ImageView avatarView = view.findViewById(R.id.avatar);
            avatarView.setImageDrawable(null);
            model.get(AVATAR_PROVIDER).onResult(avatarView);
        }
    }
}
