// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.AVATAR_PROVIDER;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.FAVICON_PROVIDER;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListProperties.TITLE_TEXT;

import android.text.TextPaint;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the single recent activity row UI. */
@NullMarked
class RecentActivityListViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (TITLE_TEXT == propertyKey) {
            ((TextView) view.findViewById(R.id.title)).setText(model.get(TITLE_TEXT));
        } else if (DESCRIPTION_AND_TIMESTAMP_TEXT == propertyKey) {
            TextView descriptionAndTimestampView = view.findViewById(R.id.description);
            DescriptionAndTimestamp descriptionAndTimestamp =
                    model.get(DESCRIPTION_AND_TIMESTAMP_TEXT);
            setDescriptionAndTimestamp(
                    descriptionAndTimestampView,
                    descriptionAndTimestamp.description,
                    descriptionAndTimestamp.separator,
                    descriptionAndTimestamp.timestamp,
                    descriptionAndTimestamp.descriptionFullTextResId);
        } else if (ON_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ON_CLICK_LISTENER));
        } else if (FAVICON_PROVIDER == propertyKey) {
            Callback faviconProvider = model.get(FAVICON_PROVIDER);
            ImageView faviconView = view.findViewById(R.id.favicon);
            if (faviconProvider == null) {
                faviconView.setVisibility(View.GONE);
            } else {
                faviconView.setImageDrawable(null);
                faviconProvider.onResult(faviconView);
            }
        } else if (AVATAR_PROVIDER == propertyKey) {
            ImageView avatarView = view.findViewById(R.id.avatar);
            avatarView.setImageDrawable(null);
            model.get(AVATAR_PROVIDER).onResult(avatarView);
        }
    }

    private static void setDescriptionAndTimestamp(
            TextView descriptionTimestampView,
            String description,
            String separator,
            String timestamp,
            int descriptionFullTextResId) {

        // Measure available width for the TextView.
        descriptionTimestampView.post(
                () -> {
                    if (TextUtils.isEmpty(description)) {
                        descriptionTimestampView.setText(timestamp);
                        return;
                    }

                    TextPaint paint = descriptionTimestampView.getPaint();
                    int totalWidth =
                            descriptionTimestampView.getWidth()
                                    - descriptionTimestampView.getPaddingLeft()
                                    - descriptionTimestampView.getPaddingRight();

                    // Measure the width needed for separator and timestamp.
                    float separatorAndTimestampWidth = paint.measureText(separator + timestamp);

                    // Calculate available width for description.
                    int descriptionAvailableWidth =
                            totalWidth - (int) Math.ceil(separatorAndTimestampWidth);

                    // Ellipsize the description.
                    String truncatedDescription =
                            (String)
                                    TextUtils.ellipsize(
                                            description,
                                            paint,
                                            descriptionAvailableWidth,
                                            TextUtils.TruncateAt.END);

                    // Combine and set the final text.
                    String fullDescriptionText =
                            descriptionTimestampView
                                    .getContext()
                                    .getString(
                                            descriptionFullTextResId,
                                            truncatedDescription,
                                            separator,
                                            timestamp);
                    descriptionTimestampView.setText(fullDescriptionText);
                });
    }
}
