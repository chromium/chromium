// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * This Controller for the auxiliary search.
 */
public class AuxiliarySearchControllerFactory {
    public static AuxiliarySearchController createAuxiliarySearchController(Profile profile, TabModelSelector tabModelSelector) {
        return new AuxiliarySearchControllerImpl(profile, tabModelSelector);
    }
}
