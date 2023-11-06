// Copyright 2021 The Chromium Authors
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
    private static boolean sTargetsAtLeastT;

    /** Rests the changes made to static state. */
    @Resetter
    public static void reset() {
        sTargetsAtLeastT = false;
    }

    /** Whether the current build is targeting at least T. */
    @Implementation
    public static boolean targetsAtLeastT() {
        return sTargetsAtLeastT;
    }

    /** Sets whether the current build is targeting at least T. */
    public static void setTargetsAtLeastT(boolean targetsAtLeastT) {
        sTargetsAtLeastT = targetsAtLeastT;
    }
}
