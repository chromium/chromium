// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * This is the Factory for the auxiliary search.
 */
public class AuxiliarySearchControllerFactory {
    public static @Nullable AuxiliarySearchController createAuxiliarySearchController(
            Profile profile, TabModelSelector tabModelSelector) {
        AuxiliarySearchHooks hooks = AuxiliarySearchHooksImpl.getInstance();
        if (!hooks.isEnabled()) return null;

        return hooks.createAuxiliarySearchController(profile, tabModelSelector);
    }
}
