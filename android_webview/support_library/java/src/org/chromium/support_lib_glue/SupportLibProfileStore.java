// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.webview.chromium.ProfileStore;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.ProfileStoreBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;
import java.util.List;

/**
 * The support-lib glue implementation for ProfileStore which is being used to manage Profile
 * operations (creation and deletion).
 */
@Lifetime.Singleton
public class SupportLibProfileStore implements ProfileStoreBoundaryInterface {
    private final ProfileStore mImpl;

    public SupportLibProfileStore(@NonNull ProfileStore profileStore) {
        mImpl = profileStore;
    }

    @Override
    @NonNull
    public /* ProfileBoundaryInterface */ InvocationHandler getOrCreateProfile(
            @NonNull String name) {
        recordApiCall(ApiCall.GET_OR_CREATE_PROFILE);
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibProfile(mImpl.getOrCreateProfile(name)));
    }

    @Override
    @Nullable
    public /* ProfileBoundaryInterface */ InvocationHandler getProfile(@NonNull String name) {
        recordApiCall(ApiCall.GET_PROFILE);
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibProfile(mImpl.getProfile(name)));
    }

    @Override
    @NonNull
    public List<String> getAllProfileNames() {
        recordApiCall(ApiCall.GET_ALL_PROFILE_NAMES);
        return mImpl.getAllProfileNames();
    }

    @Override
    public boolean deleteProfile(@NonNull String name) {
        recordApiCall(ApiCall.DELETE_PROFILE);
        return mImpl.deleteProfile(name);
    }
}
