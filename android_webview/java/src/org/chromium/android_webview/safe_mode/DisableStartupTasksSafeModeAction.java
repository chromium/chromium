// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_mode;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.build.annotations.NullMarked;

/** A {@link SafeModeAction} to disable the experiment to run WebView startup asynchronously. */
@NullMarked
public class DisableStartupTasksSafeModeAction extends SafeModeAction {
    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DISABLE_STARTUP_TASKS_LOGIC;

    @Override
    public String getId() {
        return ID;
    }
}
