// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * State in which the tab was created. This can be used in metric accounting - e.g. to
 * distinguish reasons for a tab to be restored upon first display.
 */
@IntDef({TabCreationState.LIVE_IN_FOREGROUND, TabCreationState.LIVE_IN_BACKGROUND,
        TabCreationState.FROZEN_ON_RESTORE, TabCreationState.FROZEN_FOR_LAZY_LOAD})
@Retention(RetentionPolicy.SOURCE)
public @interface TabCreationState {
    int LIVE_IN_FOREGROUND = 0;
    int LIVE_IN_BACKGROUND = 1;
    int FROZEN_ON_RESTORE = 2;
    int FROZEN_FOR_LAZY_LOAD = 3;
}
