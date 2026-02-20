// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import androidx.annotation.LayoutRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Switches the main layout based on feature flags.
 *
 * <p>TODO(http://crbug.com/485940935): Delete this.
 *
 * @deprecated This is temporary class supporting {@code
 *     main_forked_with_secondary_ui_container.xml}. Please don't add new logic here.
 */
@Deprecated
@NullMarked
public final class MainLayoutSwitcher {

    private MainLayoutSwitcher() {}

    /** Returns the resource ID for the main layout. */
    @LayoutRes
    public static int getMainLayoutRes() {
        return ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()
                ? R.layout.main_forked_with_secondary_ui_container
                : R.layout.main;
    }
}
