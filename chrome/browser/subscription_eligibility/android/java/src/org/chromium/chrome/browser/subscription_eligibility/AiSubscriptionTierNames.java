// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscription_eligibility;

import androidx.annotation.VisibleForTesting;

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

    /**
     * Returns whether {@code tier} is presented as an AI subscription tier at all, e.g. with the AI
     * tier ring. False for no subscription and for tiers excluded by the server.
     */
    public static boolean shouldShowDecoration(int tier) {
        return AiSubscriptionTierNamesJni.get().shouldShowDecoration(tier);
    }

    /** Returns the short name of {@code tier}, or an empty string if the tier has no name. */
    public static String getShortName(int tier) {
        return AiSubscriptionTierNamesJni.get().getShortName(tier);
    }

    /**
     * Returns the full name of {@code tier}, for accessibility, or an empty string if the tier has
     * none (e.g. it's unknown, or the server overrides it with an empty name).
     */
    public static String getFullName(int tier) {
        return AiSubscriptionTierNamesJni.get().getFullName(tier);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        boolean shouldShowDecoration(int tier);

        @JniType("std::u16string")
        String getShortName(int tier);

        @JniType("std::u16string")
        String getFullName(int tier);
    }
}
