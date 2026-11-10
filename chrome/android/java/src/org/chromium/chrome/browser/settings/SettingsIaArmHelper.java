// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.annotation.StringDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Holds the layout of the top-level settings page used when SettingsIaReorganization is enabled,
 * for the experiment arm selected by the feature's "arm" param.
 */
@NullMarked
public final class SettingsIaArmHelper {
    /** Experiment arms of SettingsIaReorganization, set by its "arm" param. */
    @StringDef({Arm.DIRECTION_1, Arm.DIRECTION_2, Arm.DIRECTION_3})
    @Retention(RetentionPolicy.SOURCE)
    @interface Arm {
        String DIRECTION_1 = "direction_1";
        String DIRECTION_2 = "direction_2";
        String DIRECTION_3 = "direction_3";
    }

    private SettingsIaArmHelper() {}

    /** Returns the experiment arm, or {@link Arm#DIRECTION_1} if the param is not a known arm. */
    static @Arm String getArm() {
        String arm = ChromeFeatureList.sSettingsIaReorganizationArm.getValue();
        switch (arm) {
            case Arm.DIRECTION_2:
            case Arm.DIRECTION_3:
                return arm;
            default:
                return Arm.DIRECTION_1;
        }
    }
}
