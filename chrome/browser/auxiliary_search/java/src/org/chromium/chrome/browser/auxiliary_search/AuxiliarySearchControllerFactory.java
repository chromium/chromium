// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** This is the Factory for the auxiliary search. */
public class AuxiliarySearchControllerFactory {
    public static @Nullable AuxiliarySearchController createAuxiliarySearchController(
            @NonNull Context context,
            @NonNull Profile profile,
            @Nullable TabModelSelector tabModelSelector) {
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        if (hooks == null || !hooks.isEnabled()) {
            return null;
        }

        if (ChromeFeatureList.sAndroidAppIntegrationV2.isEnabled()
                && android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            return new AuxiliarySearchControllerImpl(context, profile, tabModelSelector);
        }

        return hooks.createAuxiliarySearchController(context, profile, tabModelSelector);
    }
}
