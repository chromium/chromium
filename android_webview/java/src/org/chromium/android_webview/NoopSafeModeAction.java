// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.base.Log;

/** A {@link SafeModeAction} that has no effect. */
@Lifetime.Singleton
public class NoopSafeModeAction implements SafeModeAction {
    private static final String TAG = "WebViewSafeMode";
    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.NOOP;

    @NonNull
    @Override
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        // This is intentionally no operation as this action is meant for testing purposes only.
        Log.i(TAG, "NOOP action executed");
        return true;
    }
}
