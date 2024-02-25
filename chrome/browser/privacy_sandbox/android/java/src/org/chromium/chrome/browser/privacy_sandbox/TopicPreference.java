// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.settings.ImageButtonPreference;

/** A Preference to represent a Topic. */
public class TopicPreference extends ImageButtonPreference {
    private final @NonNull Topic mTopic;

    public TopicPreference(Context context, @NonNull Topic topic) {
        super(context);
        mTopic = topic;
        setTitle(topic.getName());
    }

    public @NonNull Topic getTopic() {
        return mTopic;
    }
}
