// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.commerce.core.ShoppingService;

/** A means of acquiring a handle to the ShoppingService. */
@JNINamespace("commerce")
public final class ShoppingServiceFactory {
    private static ShoppingService sShoppingServiceForTesting;

    /** Make it impossible to build an instance of this class. */
    private ShoppingServiceFactory() {}

    /**
     * Get the shopping service for the specified profile.
     * @param profile The profile to get the service for.
     * @return The shopping service.
     */
    public static ShoppingService getForProfile(Profile profile) {
        if (sShoppingServiceForTesting != null) {
            return sShoppingServiceForTesting;
        }
        return ShoppingServiceFactoryJni.get().getForProfile(profile);
    }

    public static void setShoppingServiceForTesting(ShoppingService shoppingService) {
        sShoppingServiceForTesting = shoppingService;
        ResettersForTesting.register(() -> sShoppingServiceForTesting = null);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        ShoppingService getForProfile(@JniType("Profile*") Profile profile);
    }
}
