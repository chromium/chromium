// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

/** Data representation of an actor task row displayed in the task list menu/bubble. */
@JNINamespace("glic")
@NullMarked
public class ActorTaskRowData {
    public final int taskId;
    public final String title;
    public final String subtitle;
    public final boolean isEnabled;
    public final boolean needsReview;

    /** Android ID of the tab associated with the task, or {@link Tab#INVALID_TAB_ID} if none. */
    public final int tabId;

    /** Constructs an {@link ActorTaskRowData} instance. */
    @CalledByNative
    public ActorTaskRowData(
            int taskId,
            @JniType("std::string") String title,
            @JniType("std::string") String subtitle,
            boolean isEnabled,
            boolean needsReview,
            int tabId) {
        this.taskId = taskId;
        this.title = title;
        this.subtitle = subtitle;
        this.isEnabled = isEnabled;
        this.needsReview = needsReview;
        this.tabId = tabId;
    }
}
