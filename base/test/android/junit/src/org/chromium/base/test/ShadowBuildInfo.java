// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.BuildInfo;

/** Shadow class of {@link BuildInfo} */
@Implements(BuildInfo.class)
public class ShadowBuildInfo {
    private static boolean sIsAtLeastS;

    /** Rests the changes made to static state. */
    @Resetter
    public static void reset() {
        sIsAtLeastS = false;
    }

    /** Whether the current build is considered to be at least S. */
    @Implementation
    public static boolean isAtLeastS() {
        return sIsAtLeastS;
    }

    /** Sets whether current Android version is at least S. */
    public static void setIsAtLeastS(boolean isAtLeastS) {
        sIsAtLeastS = isAtLeastS;
    }
}
