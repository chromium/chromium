// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import android.support.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * An annotation for setting the VrCore head tracking service's tracking mode during pre-test setup.
 *
 * The benefit of setting the mode this way instead of via HeadTrackingUtils during a test is that
 * starting services is asynchronous with no good way of waiting until whatever the service does
 * takes effect. When set during a test, the test must idle long enough to safely assume that the
 * service has taken effect. When applied during test setup, the Chrome startup period acts as the
 * wait, as Chrome startup is slow enough that it's safe to assume the service has started by the
 * time Chrome is ready.
 *
 * For example, the following would cause a test to start with its head position locked looking
 * straight forward:
 *     <code>
 *     @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
 *     </code>
 * If a test is not annotated with this, it will use whatever mode is currently set. This should
 * usually be the normal, sensor-based tracker, but is not guaranteed.
 */
@Target({ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface HeadTrackingMode {
    @IntDef({SupportedMode.FROZEN, SupportedMode.SWEEP, SupportedMode.ROTATE,
            SupportedMode.CIRCLE_STRAFE, SupportedMode.MOTION_SICKNESS})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface SupportedMode {
        int FROZEN = 0; // Locked looking straight forward.
        int SWEEP = 1; // Rotates back and forth horizontally in a 180 degree arc.
        int ROTATE = 2; // Rotates 360 degrees.
        int CIRCLE_STRAFE = 3; // Rotates 360 degrees, and if 6DOF is supported, changes position.
        int MOTION_SICKNESS = 4; // Moves in a figure-eight-like pattern.
    }

    /**
     * @return The supported mode.
     */
    public @SupportedMode int value();
}
