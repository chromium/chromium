// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.robolectric.annotation.Config;
import org.robolectric.internal.bytecode.InstrumentationConfiguration;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.TimeoutTimer;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.testing.local.ChromiumAndroidConfigurer;

/** Tells Robolectric which classes to exclude from its sandbox. */
@ServiceImpl(ChromiumAndroidConfigurer.ExtraConfiguration.class)
public class BaseRobolectricAndroidConfigurer
        implements ChromiumAndroidConfigurer.ExtraConfiguration {
    @Override
    public void withConfig(InstrumentationConfiguration.Builder builder, Config config) {
        // HelperTestRunner is already not acquired when initially created, but listing it here
        // means it will still not be acquired when accessed from within the sandbox.
        builder.doNotAcquireClass(BaseRobolectricTestRunner.HelperTestRunner.class)
                // Requires access to non-fake SystemClock.
                .doNotAcquireClass(TimeoutTimer.class)
                // Annotations used by the test runner itself to set up feature flags.
                .doNotAcquireClass(EnableFeatures.class)
                .doNotAcquireClass(DisableFeatures.class)
                // Called from outside of sandbox classloader in BaseRobolectricTestRunner.
                .doNotAcquireClass(ResettersForTesting.class);
    }
}
