// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/** Interface for implementing a generic tab action. */
@NullMarked
public interface TabActionListener {
    /**
     * Runs the action for the given {@code view} and {@code tabId}.
     *
     * @param view {@link View} for the tab.
     * @param tabId ID of the tab.
     * @param triggeringMotion {@link MotionEventInfo} that triggered the action; it will be {@code
     *     null} if the action is not a result of direct interpretation of {@link MotionEvent}s. For
     *     example, this parameter will be {@code null} if the action is run by a {@link
     *     android.view.View.OnClickListener} where {@link MotionEvent} is not available.
     */
    void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion);

    /**
     * Runs the action for the given {@code view} and tab group {@code syncId}.
     *
     * @see #run(View, int, MotionEventInfo)
     */
    void run(View view, String syncId, @Nullable MotionEventInfo triggeringMotion);
}
