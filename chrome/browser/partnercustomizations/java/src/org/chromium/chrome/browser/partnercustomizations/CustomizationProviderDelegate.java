// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import androidx.annotation.Nullable;

/** Delegate used to retrieve information from the context provider about partner customization. */
public interface CustomizationProviderDelegate {
    /** Returns the homepage string or null if none is available. */
    @Nullable
    String getHomepage();

    /** Returns whether incognito mode is disabled. */
    boolean isIncognitoModeDisabled();

    /** Returns whether bookmark editing is disabled. */
    boolean isBookmarksEditingDisabled();
}
