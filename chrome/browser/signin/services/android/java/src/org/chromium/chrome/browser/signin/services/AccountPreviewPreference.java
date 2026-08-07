// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.protocol.SyncEnums.DeviceFormFactor;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Arrays;
import java.util.Objects;

/**
 * Java counterpart to the C++ AccountPreviewPreference struct, holding preferred account and data
 * types.
 */
@JNINamespace("signin")
@NullMarked
public final class AccountPreviewPreference {
    private final GaiaId mGaiaId;
    private final @DataType int[] mPreferredDataTypes;
    private final DeviceFormFactor mOtherDeviceFormFactor;

    public AccountPreviewPreference(
            GaiaId gaiaId,
            @DataType int[] preferredDataTypes,
            DeviceFormFactor otherDeviceFormFactor) {
        mGaiaId = Objects.requireNonNull(gaiaId);
        mPreferredDataTypes = Objects.requireNonNull(preferredDataTypes).clone();
        mOtherDeviceFormFactor = Objects.requireNonNull(otherDeviceFormFactor);
    }

    @CalledByNative
    public AccountPreviewPreference(
            @JniType("GaiaId") GaiaId gaiaId,
            @JniType("std::vector<syncer::DataType>") @DataType int[] preferredDataTypes,
            @JniType("sync_pb::SyncEnums_DeviceFormFactor") int otherDeviceFormFactor) {
        this(
                gaiaId,
                preferredDataTypes,
                Objects.requireNonNullElse(
                        DeviceFormFactor.forNumber(otherDeviceFormFactor),
                        DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED));
    }

    public GaiaId getGaiaId() {
        return mGaiaId;
    }

    public @DataType int[] getPreferredDataTypes() {
        return mPreferredDataTypes.clone();
    }

    public DeviceFormFactor getOtherDeviceFormFactor() {
        return mOtherDeviceFormFactor;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof AccountPreviewPreference preference)) return false;
        return Objects.equals(mGaiaId, preference.mGaiaId)
                && Arrays.equals(mPreferredDataTypes, preference.mPreferredDataTypes)
                && mOtherDeviceFormFactor == preference.mOtherDeviceFormFactor;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mGaiaId, Arrays.hashCode(mPreferredDataTypes), mOtherDeviceFormFactor);
    }
}
