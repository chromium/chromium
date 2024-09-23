// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.chromium.chrome.browser.tab.Tab;

/** Utility classes related to the translate feature. */
public class TranslateUtils {
    /**
     * Returns true iff the content displayed in the current tab can be translated.
     *
     * @param tab The tab in question.
     */
    public static boolean canTranslateCurrentTab(Tab tab) {
        return canTranslateCurrentTab(tab, false);
    }

    /**
     * Overloaded canTranslateCurrentTab. Logging should only be performed when this method is
     * called to show the translate menu item.
     *
     * @param tab The tab in question.
     * @param menuLogging Whether logging should be performed in this check.
     */
    public static boolean canTranslateCurrentTab(Tab tab, boolean menuLogging) {
        return !tab.isNativePage()
                && tab.getWebContents() != null
                && TranslateBridge.canManuallyTranslate(tab, menuLogging);
    }
}
