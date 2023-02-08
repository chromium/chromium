// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.view.View;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Suggestions UI Delegate for constructing the TileGroup. */
public class MostVisitedSuggestionsUiDelegate extends SuggestionsUiDelegateImpl {
    private final View mView;

    public MostVisitedSuggestionsUiDelegate(View view,
            SuggestionsNavigationDelegate navigationDelegate, Profile profile,
            SnackbarManager snackbarManager) {
        super(navigationDelegate, profile, /*host=*/null, snackbarManager);
        mView = view;
    }

    @Override
    public boolean isVisible() {
        return mView.getVisibility() == View.VISIBLE
                && mView.findViewById(org.chromium.chrome.R.id.mv_tiles_layout).getVisibility()
                == View.VISIBLE;
    }
}