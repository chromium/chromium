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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** This is the Factory for the auxiliary search. */
public class AuxiliarySearchControllerFactory {
    @Nullable private final AuxiliarySearchHooks mHooks;

    @Nullable private AuxiliarySearchHooks mHooksForTesting;

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

        if (ChromeFeatureList.sAndroidAppIntegrationV2.isEnabled()
                && android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            return new AuxiliarySearchControllerImpl(context, profile, tabModelSelector);
        }

        return createAuxiliarySearchControllerImp(
                context,
                profile,
                tabModelSelector,
                mHooksForTesting == null ? mHooks : mHooksForTesting);
    }

    public void setSchemaTypeVisibilityForPackage(
            @NonNull SetDocumentClassVisibilityForPackageCallback callback) {
        if (!isEnabled()) {
            return;
        }

        mHooks.setSchemaTypeVisibilityForPackage(callback);
    }

    private @Nullable AuxiliarySearchController createAuxiliarySearchControllerImp(
            @NonNull Context context,
            @NonNull Profile profile,
            @Nullable TabModelSelector tabModelSelector,
            @NonNull AuxiliarySearchHooks hooks) {
        return hooks.createAuxiliarySearchController(context, profile, tabModelSelector);
    }

    public void setHooksForTesting(AuxiliarySearchHooks instanceForTesting) {
        mHooksForTesting = instanceForTesting;
        ResettersForTesting.register(() -> mHooksForTesting = null);
    }
}
