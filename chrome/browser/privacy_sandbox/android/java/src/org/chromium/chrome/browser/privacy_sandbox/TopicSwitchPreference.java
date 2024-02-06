// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;

public class TopicSwitchPreference extends ChromeSwitchPreference {
    private final @NonNull Topic mTopic;

    public TopicSwitchPreference(Context context, @NonNull Topic topic) {
        super(context);
        mTopic = topic;
        setTitle(topic.getName());
        setSummary(topic.getDescription());
        updateIcon();
    }

    public Topic getTopic() {
        return mTopic;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        holder.setDividerAllowedBelow(false);

        // Manually apply ListItemStartIcon style to draw the outer circle in the right size.
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);
    }

    private void updateIcon() {
        int iconId = TopicsUtils.getIconResourceIdForTopic(getContext(), mTopic);
        setIcon((iconId != 0) ? iconId : R.drawable.topic_taxonomy_placeholder);
    }
}
