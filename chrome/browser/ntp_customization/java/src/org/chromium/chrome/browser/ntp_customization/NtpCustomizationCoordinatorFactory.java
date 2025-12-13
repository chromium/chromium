// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.Supplier;

/**
 * A factory for creating and managing the lifecycle of a single {@link
 * NtpCustomizationCoordinator}. This factory ensures that at most one coordinator instance exists
 * at any time across the application.
 */
@NullMarked
public class NtpCustomizationCoordinatorFactory {

    private static @Nullable NtpCustomizationCoordinatorFactory sInstanceForTesting;

    // The single, active coordinator instance. Its lifecycle is managed by this factory.
    private @Nullable NtpCustomizationCoordinator mCoordinator;

    /** Static inner class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpCustomizationCoordinatorFactory sInstance =
                new NtpCustomizationCoordinatorFactory();
    }

    /** Returns the singleton instance of NtpCustomizationCoordinatorFactory. */
    public static NtpCustomizationCoordinatorFactory getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.sInstance;
    }

    /** Private constructor to prevent direct instantiation. */
    NtpCustomizationCoordinatorFactory() {}

    /**
     * Creates a new NtpCustomizationCoordinator. If an existing one is active, it will be destroyed
     * first.
     *
     * @return The newly created coordinator.
     */
    public NtpCustomizationCoordinator create(
            Context context,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable Profile> profileSupplier,
            @NtpCustomizationCoordinator.BottomSheetType int bottomSheetType) {
        // Destroys any previously existing coordinator to ensure only one is active.
        if (mCoordinator != null) {
            mCoordinator.dismissBottomSheet();
            mCoordinator = null;
        }

        mCoordinator =
                new NtpCustomizationCoordinator(
                        context, bottomSheetController, profileSupplier, bottomSheetType);
        return mCoordinator;
    }

    /**
     * Destroys the active coordinator if it exists and the provided instance matches the active
     * one.
     *
     * @param coordinator The coordinator instance requesting destruction.
     */
    public void onNtpCustomizationCoordinatorDestroyed(NtpCustomizationCoordinator coordinator) {
        // Only sets mCoordinator to null if the coordinator being destroyed is the one we currently
        // have stored.
        if (mCoordinator == coordinator) {
            mCoordinator = null;
        }
    }

    /**
     * @return The currently active coordinator instance, or null if none exists.
     */
    @Nullable NtpCustomizationCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }

    /**
     * Sets the coordinator instance for testing purposes.
     *
     * @param coordinator The coordinator to set as the active one.
     */
    void setCoordinatorForTesting(NtpCustomizationCoordinator coordinator) {
        mCoordinator = coordinator;
    }

    /**
     * Sets a NtpCustomizationCoordinatorFactory instance for testing.
     *
     * @param instance The instance to set for testing.
     */
    public static void setInstanceForTesting(
            @Nullable NtpCustomizationCoordinatorFactory instance) {
        var oldValue = sInstanceForTesting;
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = oldValue);
    }
}
