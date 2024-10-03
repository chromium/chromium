// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** This is the Factory for the auxiliary search. */
public class AuxiliarySearchControllerFactory {
    // Todo(b/370478696): Remove this API once the chromium code changes.
    public static @Nullable AuxiliarySearchController createAuxiliarySearchController(
            Profile profile, TabModelSelector tabModelSelector) {
        return createAuxiliarySearchController(
                ContextUtils.getApplicationContext(), profile, tabModelSelector);
    }

    public static @Nullable AuxiliarySearchController createAuxiliarySearchController(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull TabModelSelector tabModelSelector) {
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        if (hooks == null || !hooks.isEnabled()) {
            return null;
        }

        return hooks.createAuxiliarySearchController(context, profile, tabModelSelector);
    }
}
