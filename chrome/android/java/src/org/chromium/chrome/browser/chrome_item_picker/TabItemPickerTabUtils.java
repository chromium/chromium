// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxTabUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.utilities.OnDemandBackgroundTabCaptureConfig;

/** Tab state helpers shared by the tab item picker components. */
@NullMarked
final class TabItemPickerTabUtils {
    private TabItemPickerTabUtils() {}

    /**
     * Returns whether the tab already has live, loaded content and therefore needs no on-demand
     * load.
     *
     * <p>{@link FuseboxTabUtils#isTabActive} only verifies that a renderer exists. A tab whose
     * on-demand load was cancelled keeps its renderer alive but holds no loaded content, so it must
     * additionally be treated as contentless while it is marked for reload.
     *
     * @param tab The tab to check, or null.
     */
    static boolean hasLoadedContent(@Nullable Tab tab) {
        if (!FuseboxTabUtils.isTabActive(tab)) {
            return false;
        }
        if (!OnDemandBackgroundTabCaptureConfig.isCancelLoadOnDeselectionEnabled()) {
            return true;
        }
        return !assumeNonNull(tab).needsReload();
    }
}
