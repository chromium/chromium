// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;

/** Delegate to hold search provider information for the New Tab Page. */
@NullMarked
public class SearchProviderInfoDelegate {
    private boolean mSearchProviderHasLogo = true;
    private boolean mSearchProviderIsGoogle;
    private boolean mShowingNonStandardGoogleLogo;

    /**
     * Sets whether the search provider has a logo and whether it is Google.
     *
     * @param hasLogo Whether the search provider has a logo.
     * @param isGoogle Whether the search provider is Google.
     * @return True if any parameter is changed, false otherwise.
     */
    public boolean setSearchProviderInfo(boolean hasLogo, boolean isGoogle) {
        if (mSearchProviderHasLogo == hasLogo && mSearchProviderIsGoogle == isGoogle) {
            return false;
        }

        mSearchProviderHasLogo = hasLogo;
        mSearchProviderIsGoogle = isGoogle;
        if (!isGoogle) {
            mShowingNonStandardGoogleLogo = false;
        }
        return true;
    }

    /** Returns whether the search provider has a logo. */
    public boolean getSearchProviderHasLogo() {
        return mSearchProviderHasLogo;
    }

    /** Sets whether the search provider has a logo. */
    public void setSearchProviderHasLogo(boolean hasLogo) {
        mSearchProviderHasLogo = hasLogo;
    }

    /** Returns whether the search provider is Google. */
    public boolean getSearchProviderIsGoogle() {
        return mSearchProviderIsGoogle;
    }

    /** Sets whether the search provider is Google. */
    public void setSearchProviderIsGoogle(boolean isGoogle) {
        mSearchProviderIsGoogle = isGoogle;
        if (!mSearchProviderIsGoogle) {
            mShowingNonStandardGoogleLogo = false;
        }
    }

    /** Returns whether a non-standard Google logo (e.g., doodle) is being shown. */
    public boolean getShowingNonStandardGoogleLogo() {
        return mShowingNonStandardGoogleLogo;
    }

    /** Sets whether a non-standard Google logo (e.g., doodle) is being shown. */
    public void setShowingNonStandardGoogleLogo(boolean showingNonStandardGoogleLogo) {
        mShowingNonStandardGoogleLogo = showingNonStandardGoogleLogo;
    }
}
