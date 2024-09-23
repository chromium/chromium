// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** Provides access to {@link Profile} specific {@link OptimizationGuideBridge} instances. */
public class OptimizationGuideBridgeFactory {
    /** Return the {@link OptimizationGuideBridge} associated with the given {@link Profile}. */
    @Nullable
    public static OptimizationGuideBridge getForProfile(Profile profile) {
        return OptimizationGuideBridgeFactoryJni.get().getForProfile(profile);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        OptimizationGuideBridge getForProfile(@JniType("Profile*") Profile profile);
    }
}
