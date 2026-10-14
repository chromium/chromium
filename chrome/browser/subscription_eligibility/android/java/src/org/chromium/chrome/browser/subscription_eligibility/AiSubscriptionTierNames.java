// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscription_eligibility;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * Display names of the AI subscription tiers. Backed by the same C++ code as Desktop, so names and
 * server-side overrides are shared across platforms.
 *
 * <p>{@code tier} is the value returned by {@link
 * SubscriptionEligibilityService#getAiSubscriptionTier()}.
 */
@NullMarked
public final class AiSubscriptionTierNames {
    private AiSubscriptionTierNames() {}

    /** Returns the short name of {@code tier}, or an empty string if the tier has no name. */
    public static String getShortName(int tier) {
        return AiSubscriptionTierNamesJni.get().getShortName(tier);
    }

    /**
     * Returns the full name of {@code tier}, or an empty string if the tier is unknown or if its
     * short name is overridden.
     */
    public static String getFullName(int tier) {
        return AiSubscriptionTierNamesJni.get().getFullName(tier);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::u16string")
        String getShortName(int tier);

        @JniType("std::u16string")
        String getFullName(int tier);
    }
}
