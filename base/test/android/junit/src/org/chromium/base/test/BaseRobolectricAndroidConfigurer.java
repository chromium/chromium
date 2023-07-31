// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import com.google.auto.service.AutoService;

import org.robolectric.annotation.Config;
import org.robolectric.config.AndroidConfigurer;
import org.robolectric.internal.bytecode.InstrumentationConfiguration;
import org.robolectric.internal.bytecode.ShadowProviders;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.util.TimeoutTimer;

/**
 * Tells Robolectric which classes to exclude from its sandbox.
 */
@AutoService(AndroidConfigurer.class)
public class BaseRobolectricAndroidConfigurer extends AndroidConfigurer {
    public BaseRobolectricAndroidConfigurer(ShadowProviders shadowProviders) {
        super(shadowProviders);
    }

    @Override
    public void withConfig(InstrumentationConfiguration.Builder builder, Config config) {
        super.withConfig(builder, config);
        builder.doNotAcquireClass(BaseRobolectricTestRunner.HelperTestRunner.class)
                // Requires access to non-fake SystemClock.
                .doNotAcquireClass(TimeoutTimer.class)
                // Called from outside of sandbox classloader in BaseRobolectricTestRunner.
                .doNotAcquireClass(ResettersForTesting.class);
    }
}
