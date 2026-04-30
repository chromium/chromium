// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.origin_trial;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.build.annotations.NullMarked;

/** A {@link SafeModeAction} to disable all origin trials */
@NullMarked
public class DisableOriginTrialsSafeModeAction extends SafeModeAction {
    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DISABLE_ORIGIN_TRIALS;

    @Override
    public String getId() {
        return ID;
    }
}
