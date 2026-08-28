// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.build.annotations.AlwaysInline;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchHostType;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchDonor.SetDocumentClassVisibilityForPackageCallback;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** This is the Factory for the auxiliary search. */
@NullMarked
public class AuxiliarySearchControllerFactory {
    private boolean mSupportMultiDataSource;

    /** It tracks whether the current device is a tablet. */
    private @TriState int mIsTablet;

    private @Nullable AuxiliarySearchController mAuxiliarySearchMultiDataController;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final AuxiliarySearchControllerFactory sInstance =
                new AuxiliarySearchControllerFactory();
    }

    /** Returns the singleton instance of AuxiliarySearchControllerFactory. */
    public static AuxiliarySearchControllerFactory getInstance() {
        return LazyHolder.sInstance;
    }

    private AuxiliarySearchControllerFactory() {
        mSupportMultiDataSource = isMultiDataTypeEnabledOnDevice();
    }

    @AlwaysInline
    private static @Nullable AuxiliarySearchHooks getHooks() {
        return ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
    }

    /** Returns whether the hook is enabled on device. */
    public boolean isEnabled() {
        AuxiliarySearchHooks hooks = getHooks();
        return hooks != null && hooks.isEnabled();
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

    /** Returns whether the multiple types of data sources is enabled on this device. */
    public boolean isMultiDataTypeEnabledOnDevice() {
        AuxiliarySearchHooks hooks = getHooks();
        return hooks != null && hooks.isMultiDataTypeEnabledOnDevice();
    }

    /** Returns whether the sharing Tabs with the system is enabled by default on the device. */
    public boolean isSettingDefaultEnabledByOs() {
        AuxiliarySearchHooks hooks = getHooks();
        return hooks != null && hooks.isSettingDefaultEnabledByOs();
    }

    /** Creates a {@link AuxiliarySearchController} instance if enabled. */
    public @Nullable AuxiliarySearchController createAuxiliarySearchController(
            Context context,
            Profile profile,
            @Nullable TabModelSelector tabModelSelector,
            @AuxiliarySearchHostType int hostType) {
        if (!isEnabled()) {
            return null;
        }

        if (mSupportMultiDataSource && hostType == AuxiliarySearchHostType.CTA) {
            if (mAuxiliarySearchMultiDataController == null) {
                mAuxiliarySearchMultiDataController =
                        new AuxiliarySearchMultiDataControllerImpl(
                                context, profile, AuxiliarySearchHostType.CTA);
            }
            return mAuxiliarySearchMultiDataController;
        }

        return new AuxiliarySearchControllerImpl(context, profile, tabModelSelector, hostType);
    }

    public void setSchemaTypeVisibilityForPackage(
            SetDocumentClassVisibilityForPackageCallback callback) {
        if (!isEnabled()) {
            return;
        }

        assumeNonNull(getHooks()).setSchemaTypeVisibilityForPackage(callback);
    }

    /**
     * Sets whether the device is a tablet. Note: this must be called before checking isEnabled().
     */
    public void setIsTablet(boolean isTablet) {
        mIsTablet = TriStateUtils.from(isTablet || mIsTablet == TriState.TRUE);
    }

    /** Gets whether the device is a tablet. */
    public boolean isTablet() {
        assert mIsTablet != TriState.NOT_SET;
        return mIsTablet == TriState.TRUE;
    }

    public @Nullable String getSupportedPackageName() {
        AuxiliarySearchHooks hooks = getHooks();
        if (hooks != null) {
            return hooks.getSupportedPackageName();
        }

        return null;
    }

    public void setSupportMultiDataSourceForTesting(boolean supportMultiDataSource) {
        boolean oldValue = mSupportMultiDataSource;
        mSupportMultiDataSource = supportMultiDataSource;
        ResettersForTesting.register(() -> mSupportMultiDataSource = oldValue);
    }

    public void resetIsTabletForTesting() {
        mIsTablet = TriState.NOT_SET;
    }
}
