// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.IntDef;

/** Indicates where a new tab was launched from. */
@IntDef({NewTabPageLaunchOrigin.UNKNOWN, NewTabPageLaunchOrigin.WEB_FEED})
public @interface NewTabPageLaunchOrigin {
    /** Unknown launch origin. Used as the default. */
    int UNKNOWN = 0;

    /** Opened from the Web Feed go to feed option. Includes post-follow snackbar and dialog. */
    int WEB_FEED = 1;
}
