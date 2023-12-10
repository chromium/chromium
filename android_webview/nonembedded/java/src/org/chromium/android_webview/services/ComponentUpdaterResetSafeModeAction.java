// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;

/** A {@link SafeModeAction} to reset Component Updater. */
public class ComponentUpdaterResetSafeModeAction implements SafeModeAction {
    private static final String TAG = "WebViewSafeMode";

    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.RESET_COMPONENT_UPDATER;

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        // Each Component Updater Service will take requisite steps on startup.
        // Do nothing here.
        return true;
    }
}
