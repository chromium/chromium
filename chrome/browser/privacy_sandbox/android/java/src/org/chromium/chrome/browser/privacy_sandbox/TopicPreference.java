// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.ImageButtonPreference;

/** A Preference to represent a Topic. */
@NullMarked
public class TopicPreference extends ImageButtonPreference {
    private final Topic mTopic;

    public TopicPreference(Context context, Topic topic) {
        super(context);
        mTopic = topic;
        setTitle(topic.getName());
    }

    public Topic getTopic() {
        return mTopic;
    }
}
