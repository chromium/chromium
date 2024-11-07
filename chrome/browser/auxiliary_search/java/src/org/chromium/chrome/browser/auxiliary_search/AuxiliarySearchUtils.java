// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchProvider.USE_LARGE_FAVICON;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.VisibleForTesting;

import java.io.File;

public class AuxiliarySearchUtils {
    @VisibleForTesting static final String TAB_DONATE_FILE_NAME = "tabs_donate";

    @VisibleForTesting
    public static int getFaviconSize(Resources resources) {
        return USE_LARGE_FAVICON.getValue()
                ? resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size)
                : resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size_small);
    }

    /** Returns the file to save the metadata for donating tabs. */
    @VisibleForTesting
    public static File getTabDonateFile(Context context) {
        return new File(context.getFilesDir(), TAB_DONATE_FILE_NAME);
    }
}
