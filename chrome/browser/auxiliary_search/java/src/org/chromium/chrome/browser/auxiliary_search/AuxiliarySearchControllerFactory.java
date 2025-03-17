// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchDonor.SetDocumentClassVisibilityForPackageCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** This is the Factory for the auxiliary search. */
public class AuxiliarySearchControllerFactory {
    @Nullable private final AuxiliarySearchHooks mHooks;

    @Nullable private AuxiliarySearchHooks mHooksForTesting;

    /** It tracks whether the current device is a tablet. */
    @Nullable private Boolean mIsTablet;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static AuxiliarySearchControllerFactory sInstance = new AuxiliarySearchControllerFactory();
    }

    /** Returns the singleton instance of AuxiliarySearchControllerFactory. */
    public static AuxiliarySearchControllerFactory getInstance() {
        return LazyHolder.sInstance;
    }

    private AuxiliarySearchControllerFactory() {
        mHooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
    }

    /** Returns whether the hook is enabled on device. */
    public boolean isEnabled() {
        if (mHooksForTesting != null) {
            return mHooksForTesting.isEnabled();
        }

        return mHooks != null && mHooks.isEnabled();
    }

    /**
     * Returns whether Tab Sharing is enabled and the device is ready to use it. This check is used
     * to control the visibility of UI like the toggle in Tabs Settings and opt in card on the magic
     * stack. This function should NOT be used to determine whether to create a Tab Sharing
     * controller.
     */
    public boolean isEnabledAndDeviceCompatible() {
        boolean consumerSchemaFound =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, false);

        return consumerSchemaFound && isEnabled();
    }

    /** Returns whether the sharing Tabs with the system is enabled by default on the device. */
    public boolean isSettingDefaultEnabledByOs() {
        if (mHooksForTesting != null) {
            return mHooksForTesting.isSettingDefaultEnabledByOs();
        }

        return mHooks != null && mHooks.isSettingDefaultEnabledByOs();
    }

    /** Creates a {@link AuxiliarySearchController} instance if enabled. */
    public @Nullable AuxiliarySearchController createAuxiliarySearchController(
            @NonNull Context context,
            @NonNull Profile profile,
            @Nullable TabModelSelector tabModelSelector) {
        if (!isEnabled()) {
            return null;
        }

        assert ChromeFeatureList.sAndroidAppIntegrationV2.isEnabled();
        return new AuxiliarySearchControllerImpl(context, profile, tabModelSelector);
    }

    public void setSchemaTypeVisibilityForPackage(
            @NonNull SetDocumentClassVisibilityForPackageCallback callback) {
        if (!isEnabled()) {
            return;
        }

        mHooks.setSchemaTypeVisibilityForPackage(callback);
    }

    /**
     * Sets whether the device is a tablet. Note: this must be called before checking isEnabled().
     */
    public void setIsTablet(boolean isTablet) {
        mIsTablet = isTablet || (mIsTablet != null && mIsTablet);
    }

    /** Gets whether the device is a tablet. */
    public boolean isTablet() {
        assert mIsTablet != null;
        return mIsTablet;
    }

    @Nullable
    public String getSupportedPackageName() {
        if (mHooksForTesting != null) {
            return mHooksForTesting.getSupportedPackageName();
        }

        if (mHooks != null) {
            return mHooks.getSupportedPackageName();
        }

        return null;
    }

    public void setHooksForTesting(AuxiliarySearchHooks instanceForTesting) {
        mHooksForTesting = instanceForTesting;
        ResettersForTesting.register(() -> mHooksForTesting = null);
    }

    public void resetIsTabletForTesting() {
        mIsTablet = null;
    }
}
