// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.experiments;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.content_public.common.Referrer;

/**
 * Manages an Ephemeral Tab, which allows a "sneak peek" at a linked page using the Overlay Panel.
 */
public class EphemeralTab {
    /** @return whether this feature is currently capable of being used. */
    public static boolean isCapable() {
        // TODO(donnd): check if all the conditions are right to support an Overlay.
        return isEnabled();
    }

    /**
     * Called when an Open operation needs to be taken.
     * @param url The URL of the page to open.
     * @param referrer The current {@link Referrer}.
     * @param isIncognito Whether the Overlay should use Incognito.
     */
    public static void onOpen(String url, Referrer referrer, boolean isIncognito) {
        // TODO(donnd): Implement.
    }

    /** @return Whether this feature is enabled. */
    private static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.EPHEMERAL_TAB);
    }
}
