// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedDialogProperties.DETAILS;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedDialogProperties.TITLE;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.feed.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Class responsible for binding the model and the view for the WebFeed dialog. */
class WebFeedDialogViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ILLUSTRATION == propertyKey) {
            ((ImageView) view.findViewById(R.id.web_feed_dialog_illustration))
                    .setImageResource(model.get(ILLUSTRATION));
        } else if (TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.web_feed_dialog_title)).setText(model.get(TITLE));
        } else if (DETAILS == propertyKey) {
            ((TextView) view.findViewById(R.id.web_feed_dialog_details))
                    .setText(model.get(DETAILS));
        }
    }
}
