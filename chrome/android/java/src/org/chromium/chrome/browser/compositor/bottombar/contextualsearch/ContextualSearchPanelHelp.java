// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Controls a section of the Panel that provides end user help messages.
 * TODO(donnd): Implement along the lines of {@code ContextualSearchPromoControl}.
 */
public class ContextualSearchPanelHelp {
    private final boolean mIsEnabled;
    private final String mHelpHeaderText;
    private final String mHelpBodyText;

    /**
     * @param context The current Android {@link Context}.
     */
    ContextualSearchPanelHelp(Context context) {
        mIsEnabled = ChromeFeatureList.isEnabled(
                ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_PANEL_HELP);
        mHelpHeaderText = context.getResources().getString(R.string.contextual_search_help_header);
        mHelpBodyText = context.getResources().getString(R.string.contextual_search_help_body);
        // TODO(donnd): Update this class and constructor to do useful work.
    }
}
