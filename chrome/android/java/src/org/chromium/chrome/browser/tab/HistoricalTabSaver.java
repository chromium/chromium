// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.WebContents;

/**
 * Saves historical tabs.
 */
public class HistoricalTabSaver {
    /**
     * Creates a historical tab from a tab being closed.
     */
    public static void createHistoricalTab(Tab tab) {
        if (tab.isFrozen()) {
            WebContentsState state = CriticalPersistedTabData.from(tab).getWebContentsState();
            if (state != null) {
                WebContents webContents =
                        WebContentsStateBridge.restoreContentsFromByteBuffer(state, true);
                if (webContents != null) {
                    createHistoricalTabFromContents(webContents);
                    webContents.destroy();
                }
            }
        } else {
            createHistoricalTabFromContents(tab.getWebContents());
        }
    }

    private static void createHistoricalTabFromContents(WebContents webContents) {
        HistoricalTabSaverJni.get().createHistoricalTabFromContents(webContents);
    }

    @NativeMethods
    interface Natives {
        void createHistoricalTabFromContents(WebContents webContents);
    }
}
