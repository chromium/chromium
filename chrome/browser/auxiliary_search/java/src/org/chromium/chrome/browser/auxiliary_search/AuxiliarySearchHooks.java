// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Provides access to internal AuxiliarySearch implementation parts, if they are available.
 */
public interface AuxiliarySearchHooks {
    /** Whether the internal components of the Auxiliary Search are available.*/
    default boolean isEnabled() {
        return false;
    }

    /** Create a AuxiliarySearchController. */
    default @Nullable AuxiliarySearchController createAuxiliarySearchController(
            Profile profile, TabModelSelector tabModelSelector) {
        return null;
    }
}
